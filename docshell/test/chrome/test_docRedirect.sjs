function handleRequest(request, response) {
  response.setStatusLine(request.httpVersion, 301, "Moved Permanently");
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  response.setHeader("Location", "http://example.org/");
  response.write("Hello world!");
}
