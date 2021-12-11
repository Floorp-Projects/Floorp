function handleRequest(request, response) {
  response.setHeader("Cache-Control", "no-cache", false);
  response.setStatusLine(request.httpVersion, 200, "OK");

  response.write(request.queryString);
}
