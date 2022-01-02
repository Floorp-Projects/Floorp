function handleRequest(request, response) {
  response.setStatusLine(request.httpVersion, 404, "Not Found");
}
