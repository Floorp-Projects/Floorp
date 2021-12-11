function handleRequest(request, response) {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Mirrored", request.getHeader("Mirror"));
  response.write(request.getHeader("Mirror"));
}
