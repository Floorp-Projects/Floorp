function handleRequest(request, response) {
  response.setStatusLine(request.httpVersion, 301, "Moved Permanently");
  response.setHeader("Location", "synthesized-redirect-twice-real-file.txt");
}
