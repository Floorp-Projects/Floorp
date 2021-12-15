function handleRequest(request, response) {
  // Only answer to http, in case request is in https let the reply time out.
  if (request.scheme === "https") {
    response.processAsync();
    return;
  }

  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Disposition", "attachment; filename=some.html");
  response.setHeader("Content-Type", "text/html");
  response.write("success!");
}
