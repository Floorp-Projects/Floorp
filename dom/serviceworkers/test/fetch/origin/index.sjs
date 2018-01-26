function handleRequest(request, response) {
  response.setStatusLine(null, 308, "Permanent Redirect");
  response.setHeader("Location", "http://example.org/tests/dom/workers/test/serviceworkers/fetch/origin/realindex.html", false);
}
