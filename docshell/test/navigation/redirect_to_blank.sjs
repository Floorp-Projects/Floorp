function handleRequest(request, response) {
  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Cache-Control", "no-store", false);
  response.setStatusLine("1.1", 302, "Found");
  response.setHeader("Location", "blank.html", false);
}
