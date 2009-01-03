function handleRequest(request, response) {
  response.setStatusLine(null, 302, "Found");
  response.setHeader("Location", "http://example.com/tests/content/base/test/bug461735-post-redirect.js", false);
}
