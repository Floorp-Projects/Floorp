function handleRequest(request, response) {
  response.setStatusLine(null, 308, "Permanent Redirect");
  response.setHeader("Access-Control-Allow-Origin", "*", false);
  response.setHeader("Location", "https://example.org/tests/dom/workers/test/serviceworkers/app-protocol/realindex.html", false);
}
