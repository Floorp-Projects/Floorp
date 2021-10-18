function handleRequest(request, response) {
  response.setStatusLine(null, 308, "Permanent Redirect");
  response.setHeader(
    "Location",
    "http://example.org/tests/dom/serviceworkers/test/fetch/requesturl/secret.html",
    false
  );
}
