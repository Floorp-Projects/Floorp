function handleRequest(request, response) {
  response.setStatusLine("1.1", 302, "Found");
  response.setHeader("Location", "http://example.org/tests/dom/workers/test/foreign.js");
}
