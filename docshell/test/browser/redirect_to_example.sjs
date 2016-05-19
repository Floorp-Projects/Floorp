function handleRequest(request, response) {
  response.setStatusLine(request.httpVersion, 302, "Moved Permanently");
  response.setHeader("Location", "http://example");
}
