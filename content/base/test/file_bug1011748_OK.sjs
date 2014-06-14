// Function to indicate HTTP 200 OK after redirect from file_bug1011748_redirect.sjs
function handleRequest(request, response) {
  response.setStatusLine(null, 200, "OK");
}
