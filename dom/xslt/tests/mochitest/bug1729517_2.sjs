function handleRequest(request, response) {
  response.write(request.hasHeader("Referer") ? "FAIL" : "PASS");
}
