function handleRequest(request, response) {
  response.setHeader("Content-Type", "text/plain", false);
  response.setHeader("Cache-Control", "no-cache", false);

  var referer = request.hasHeader("Referer") ? request.getHeader("Referer")
                                             : "";
  response.write("Referer: " + referer);
}
