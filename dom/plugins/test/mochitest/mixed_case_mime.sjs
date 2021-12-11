function handleRequest(request, response) {
  response.processAsync();
  response.setHeader("Content-Type", "image/pNG", false);

  response.write("Hello world.\n");
  response.finish();
}
