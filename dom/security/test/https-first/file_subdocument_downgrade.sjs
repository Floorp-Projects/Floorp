function handleRequest(request, response) {
  if (request.scheme === "https") {
    response.setStatusLine("1.1", 429, "Too Many Requests");
  } else {
    response.setHeader("Content-Type", "text/html", false);
    response.write("<!doctype html><html><body></body></html>");
  }
}
