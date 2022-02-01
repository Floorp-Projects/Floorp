function handleRequest(request, response) {
  // Only answer to http, in case request is in https let the reply time out.
  if (request.scheme === "https") {
    response.processAsync();
    return;
  }

  response.setHeader("Cache-Control", "no-cache", false);
  // Send some file, e.g. an image
  response.setHeader(
    "Content-Disposition",
    "attachment; filename=some-file.png"
  );
  response.setHeader("Content-Type", "image/png");
  response.write("Success!");
}
