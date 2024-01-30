#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/stat.h>

/* Socket address, Internet style. */
typedef uint16_t in_port_t;
typedef unsigned short int sa_family_t; // not relevent because we always work on 4 bytes ip address

struct ParsedURL {
    char *hostname;
    int port;
    char *filepath;
    char *fullpath;
};

// Function to check if a string is numeric
int isNumericString(const char *str) {
    while (*str) {
        if (!isdigit(*str)) {
            return 0; // Not a numeric character
        }
        str++;
    }
    return 1; // Entire string is numeric
}
// Method to create ParsedURL struct
struct ParsedURL createParsedURL(const char *url) {
    if (strncmp(url, "http://", 7) != 0) {
        fprintf(stderr, "Invalid URL format\n");
        exit(EXIT_FAILURE);
    }
    struct ParsedURL result;
    url += 7;
    // Check for ':' and a number after the first '/'
    char *firstSlash = strchr(url, '/');
    char *colonPos = strchr(url, ':');
    int is_number=0;
    //if there is a char ':' and '/' if the char ':' is before the char '/' we will check if between there is a number
    if(colonPos != NULL && firstSlash != NULL && colonPos > firstSlash){
        // Extract the substring between ':' and '/'
        int number_length = (int)(firstSlash - colonPos - 1);
        char *substring = malloc(number_length+1);
        strncpy(substring, colonPos + 1, number_length);
        substring[number_length] = '\0';
        is_number=isNumericString(substring);
        free(substring);
        if(is_number==1){ // if there is a number between ':' and '/'
            result.port = atoi((const char *)(colonPos + 1));
            result.hostname = malloc(colonPos - url + 1);
            strncpy(result.hostname, url, (int)(colonPos - url));
            result.hostname[colonPos - url] = '\0';
        }
        else{ // if there is no number between ':' and '/'
            result.port = 80;
            result.hostname = malloc(firstSlash - url + 1);
            strncpy(result.hostname, url, (int)(firstSlash - url));
            result.hostname[firstSlash - url] = '\0';
        }
    }
    //if there is a char ':' and no '/' we will check if after the ':' there is a number
    if(colonPos != NULL && firstSlash == NULL){
        is_number=isNumericString(colonPos+1);
        if(is_number==1){ // if there is a number after ':'
            // Set port and hostname based on ':' and a number
            result.port = atoi((const char *)(colonPos + 1));
            result.hostname = malloc(colonPos - url + 1);
            strncpy(result.hostname, url, (int)(colonPos - url));
            result.hostname[colonPos - url] = '\0';
        }else{// if there is no a number after ':'
            result.port = 80;
            result.hostname = malloc(strlen(url) + 1);
            strncpy(result.hostname, url, (int)strlen(url));
            result.hostname[strlen(url)] = '\0';
        }
    }
    //if there is no ':' and no '/'
    else if (colonPos == NULL  && firstSlash==NULL) {
        // Set port and hostname based on ':' and a number
        result.port = 80;
        result.hostname = malloc(strlen(url) + 1);
        strncpy(result.hostname, url, (int)strlen(url));
        result.hostname[strlen(url)] = '\0';
    } else {
        // Set default port and hostname
        result.port = 80;
        result.hostname = malloc(firstSlash - url + 1);
        strncpy(result.hostname, url, (firstSlash - url));
        result.hostname[firstSlash - url] = '\0';
    }
    if(firstSlash==NULL){
        result.filepath = malloc(2); // +1 for null terminator
        result.filepath[0]='/';
        result.filepath[1]='\0';
        // Calculate the length of the path
        int pathLength = strlen(result.hostname) + strlen(result.filepath) + 2;
        result.fullpath = malloc(pathLength);
        // Concatenate hostname and filepath to form the full path
        sprintf(result.fullpath, "%s%s", result.hostname, result.filepath);
    }else{
        result.filepath = malloc(strlen(firstSlash + 1) + 1); // +1 for null terminator
        strcpy(result.filepath, firstSlash + 1);
        result.filepath[strlen(firstSlash + 1) + 1]='\0';
        // Calculate the length of the path
        int pathLength = strlen(result.hostname) + strlen(result.filepath) + 2;
        result.fullpath = malloc(pathLength);
        // Concatenate hostname and filepath to form the full path
        sprintf(result.fullpath, "%s/%s", result.hostname, result.filepath);
    }
    return result;
}

// Method to search for the path in the linked list
int searchPath(struct ParsedURL *parsedURL) {
    // Check if the subdirectory exists
    char *path=parsedURL->fullpath;
    if(strcmp(parsedURL->filepath, "/") == 0){
        path=parsedURL->hostname;
        parsedURL->fullpath[strlen(parsedURL->fullpath)-1]='\0';
    }
    if (access(path, F_OK) == -1) {
        //TODO put commen on the print
        printf("Directory does not exist: %s\n", parsedURL->fullpath);
        return 0;
    }
    //TODO put commen on the print
    printf("Path exists: %s\n", parsedURL->fullpath);
    return 1;
}

// Method to simulate proxy response
void fileFoundResponse(struct ParsedURL *parsedURL) {
    // Open the file
    FILE *file = fopen(parsedURL->fullpath, "rb");
    if (!file) {
        fprintf(stderr, "Error opening file: %s\n", parsedURL->fullpath);
        perror("fopen");
        return;
    }
    // Get file size
    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);
    // Print HTTP response
    fflush(stdout); // Manually flush the output buffer
    printf("HTTP/1.0 200 OK\r\nContent-Length: %ld\r\n\r\n", fileSize);
    fflush(stdout); // Manually flush the output buffer
    // Print file content
    char buffer[BUFSIZ];
    size_t bytesRead;
    while ((bytesRead = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        fwrite(buffer, 1, bytesRead, stdout);
    }
    // Close the file
    fclose(file);
    ////TODO what need to be total response bytes
    printf("Total response bytes: %ld\n", fileSize);
    //fflush(stdout); // Manually flush the output buffer
}

// Helper function to create directories recursively
void create_directories(const char *path) {
    const char *start = path;
    const char *end;
    // Find the last '/'
    const char *lastSlash = strrchr(path, '/');
    // Iterate through each directory level except the last one
    while ((end = strchr(start, '/')) != NULL && end < lastSlash) {
        // Create a substring for the current directory level
        char token[end - start + 1];
        strncpy(token, start, end - start);
        token[end - start] = '\0';
        // Create the directory
        mkdir(token, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
        // Move to the next part of the path
        start = end + 1;
    }
}

// Function to find the length of a substring in a buffer
size_t find_substring_length(const char *buffer, const char *substring) {
    const char *found = strstr(buffer, substring);
    return found ? strlen(found + strlen(substring)) : 0;
}

int extract_status_code(const char *status_line) {
    int status_code = 0;
    const char *status_code_str = status_line + strspn(status_line, "HTTP/1.X ");
    while (*status_code_str >= '0' && *status_code_str <= '9') {
        status_code = status_code * 10 + (*status_code_str - '0');
        status_code_str++;
    }
    return status_code;
}

// Handle the server response
void handle_server_response(int sockfd, const struct ParsedURL *parsedURL) {
    char buffer[BUFSIZ];
    ssize_t bytesRead;
    int status_code_extracted = 0;
    int status_code=0;
    size_t content_length = 0; // Variable to store Content-Length
    int file_open =0;
    FILE *file=NULL;
    size_t totalBytesRead = 0;
    const char* content_length_position=NULL;
    while ((bytesRead = recv(sockfd, buffer, sizeof(buffer), 0)) > 0) {
        if (bytesRead < 0) {
            perror("recv failed");
            exit(EXIT_FAILURE);
        }
        // Process or print the received data
        fwrite(buffer, 1, bytesRead, stdout);

        // Extract the HTTP status code only in the first iteration
        // Extract the HTTP status code only in the first iteration
        if (!status_code_extracted) {
            status_code = extract_status_code(buffer);
            status_code_extracted = 1;
        }
        // Extract Content-Length from headers
        if (content_length == 0) {
            size_t header_length = find_substring_length(buffer, "Content-Length:");
            if (header_length > 0) {
                char *content_length_str = malloc(header_length + 1);
                if (content_length_str == NULL) {
                    perror("malloc failed");
                    exit(EXIT_FAILURE);
                }
                memcpy(content_length_str, strstr(buffer, "Content-Length:") + strlen("Content-Length:"), header_length);
                content_length_str[header_length] = '\0';
                content_length = strtol(content_length_str, NULL, 10);
                free(content_length_str);
            }
        }
        if(file_open ==0 && status_code==200){
            create_directories(parsedURL->fullpath);
            if(strcmp(parsedURL->filepath, "/") == 0){
                file = fopen(parsedURL->hostname, "wb");
            }
            else{ // Open the file for writing
                file = fopen(parsedURL->fullpath, "wb");
            }
            if (!file) {
                fprintf(stderr, "Error opening file for writing: %s\n", parsedURL->fullpath);
                perror("fopen");
                close(sockfd);
                exit(EXIT_FAILURE);
            }
            file_open=1;
        }
        if(totalBytesRead < content_length) {
            fwrite(buffer, 1, bytesRead, file);
            totalBytesRead += bytesRead;
        }
    }
    fclose(file);
    // Close the socket before exiting
    close(sockfd);
}

int establish_tcp_connection(struct ParsedURL *parsedURL){
    char host_name[strlen(parsedURL->hostname)];
    strncpy(host_name, parsedURL->hostname, sizeof(host_name) - 1);
    host_name[sizeof(host_name) - 1] = '\0';  // Ensure null-termination
    in_port_t port = parsedURL->port;
    int sockfd = -1;
    /* Create a socket with the address format of IPV4 over TCP */
    if ((sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1){
        perror("socket failed");
        exit(EXIT_FAILURE);
    }/* Use gethostbyname to translate host name to network byte order ip address */
    struct hostent* server_info = NULL;
    if ((server_info = gethostbyname(parsedURL->hostname)) == NULL) {
        perror("gethostbyname failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    /* Initialize sockaddr_in structure */
    struct sockaddr_in sock_info;
    /* Set its attributes to 0 to avoid undefined behavior */
    memset(&sock_info, 0, sizeof(struct sockaddr_in));
    /* Set the type of the address to be IPV4 */
    sock_info.sin_family = AF_INET;
    /* Set the socket's port */
    sock_info.sin_port = htons(port);
    /* Set the socket's ip */
    sock_info.sin_addr.s_addr = ((struct in_addr*)server_info->h_addr)->s_addr;
    /* connect to the server*/
    if (connect(sockfd, (struct sockaddr*)&sock_info, sizeof(sock_info)) == -1){
        perror("connect failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    const char *message = "GET %s HTTP/1.0\r\nHost: %s\r\n\r\n";
    size_t request_len = strlen(message) + strlen(parsedURL->filepath) + strlen(parsedURL->hostname) + 1; // +1 for null terminator
    char *request = (char *)malloc(request_len);
    if (request == NULL) {
        perror("Memory allocation failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    sprintf(request, message, parsedURL->filepath, parsedURL->hostname);
    fflush(stdout); // Manually flush the output buffer
    printf("GET %s HTTP/1.0\r\nHost: %s\r\n\r\n",parsedURL->filepath, parsedURL->hostname);
    fflush(stdout); // Manually flush the output buffer
    if (send(sockfd, request, strlen(request), 0) < 0) {
        perror("Send failed");
        free(request);
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    free(request);
    // Handle the server response
    handle_server_response(sockfd, parsedURL);
    return sockfd;
};

int main() {
    //"http://www.example.com:8080/path/to/resource" // path not exist
    //"http://www.example.com:8080/path/test.txt"; //path exist
    const char *url = "http://www.josephwcarrillo.com"; //path exist
    struct ParsedURL parsedURL = createParsedURL(url);
    // Print ParsedURL information
    printf("Hostname: %s,\nPort: %d,\nFilepath: %s\n", parsedURL.hostname, parsedURL.port, parsedURL.filepath);

    int res = searchPath(&parsedURL);
    int sockfd = -1;  // Declare sockfd here to ensure it's accessible outside the if block
    if(res==0){
        sockfd = establish_tcp_connection(&parsedURL);
    }
    else{
        fileFoundResponse(&parsedURL);
    }
    // Close the socket if it was not closed in establish_tcp_connection
    if (sockfd != -1) {
        close(sockfd);
    }
    // Free memory
    free(parsedURL.hostname);
    free(parsedURL.filepath);
    free(parsedURL.fullpath);

    return 0;
}
