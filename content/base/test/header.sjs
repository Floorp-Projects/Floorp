function handleRequest(request, response) {
  response.setHeader("Content-Type", "text/plain", false);
  response.setHeader("Cache-Control", "no-cache", false);

  var value = request.hasHeader("SomeHeader") ? request.getHeader("SomeHeader")
                                             : "";
  response.write("SomeHeader: " + value);
}
