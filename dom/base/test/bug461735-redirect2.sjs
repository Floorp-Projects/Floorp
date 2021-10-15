function handleRequest(request, response) {
  response.setStatusLine(null, 302, "Found");
  response.setHeader("Location", "http://mochi.test:8888/tests/dom/base/test/bug461735-post-redirect.js", false);
}
