function handleRequest(request, response) {
  response.setStatusLine(null, 302, "Found");
  response.setHeader("Location", request.queryString, false);
}
