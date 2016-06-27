function handleRequest(request, response) {
  response.setHeader("Access-Control-Allow-Origin", "*");

  if (request.queryString == 'redirect') {
    response.setStatusLine("1.1", 302, "Found");
    response.setHeader("Location", "worker_xhr_cors_redirect.sjs");
  } else {
    response.write("'hello world'");
  }
}
