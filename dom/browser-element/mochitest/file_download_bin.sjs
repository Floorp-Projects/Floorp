function handleRequest(request, response) {
  response.setHeader("Content-Type", "application/octet-stream", false);
  response.write("BIN");
}