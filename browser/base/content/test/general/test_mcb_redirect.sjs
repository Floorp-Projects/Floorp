function handleRequest(request, response) {
  var page = "<!DOCTYPE html><html><body>bug 418354</body></html>";

  var redirect = "http://example.com/browser/browser/base/content/test/general/test_mcb_redirect.js";

  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "text/html", false);
  response.setStatusLine(request.httpVersion, "302", "Found");
  response.setHeader("Location", redirect, false);
  response.write(page);
}
