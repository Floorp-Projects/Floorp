function handleRequest(request, response) {
  response.setStatusLine(null, 308, "Permanent Redirect");
  response.setHeader("Location", "https://example.org/tests/dom/workers/test/serviceworkers/fetch/origin/https/realindex.html", false);
}
