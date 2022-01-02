function handleRequest(request, response) {
  response.setHeader("Content-Type", "text/html");

  if (request.queryString == "redirect") {
    response.setStatusLine(request.httpVersion, 302, "See Other");
    response.setHeader("Location", "test_worker_performance_entries.sjs?ok");
  } else {
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.write("Hello world \\o/");
  }
}
