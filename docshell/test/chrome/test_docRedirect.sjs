function handleRequest(request, response) {
  response.setStatusLine(request.httpVersion, 301, "Moved Permanently");
  response.setHeader("Location", "http://example.org/");
  response.write("Hello world!");
}
