function handleRequest(request, response) {
  if (request.getHeader("Service-Worker") === "script") {
    response.setStatusLine("1.1", 200, "OK");
    response.setHeader("Content-Type", "text/javascript");
    response.write("// empty");
  } else {
    response.setStatusLine("1.1", 404, "Not Found");
  }
}
