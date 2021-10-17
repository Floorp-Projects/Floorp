function handleRequest(request, response) {
  var page = "<!DOCTYPE html><html><body>bug 418354 and bug 1082837</body></html>";

  if (request.queryString === "script") { 
    var redirect = "http://example.com/browser/browser/base/content/test/siteIdentity/test_mcb_redirect.js";
    response.setHeader("Cache-Control", "no-cache", false);
  } else if (request.queryString === "image_http") {
    var redirect = "http://example.com/tests/image/test/mochitest/blue.png";
    response.setHeader("Cache-Control", "max-age=3600", false);
  } else if (request.queryString === "image_redirect_http_sjs") {
    var redirect = "http://example.com/browser/browser/base/content/test/siteIdentity/test_mcb_redirect.sjs?image_redirect_https";
    response.setHeader("Cache-Control", "max-age=3600", false);
  } else if (request.queryString === "image_redirect_https") {
    var redirect = "https://example.com/tests/image/test/mochitest/blue.png";
    response.setHeader("Cache-Control", "max-age=3600", false);
  }

  response.setHeader("Content-Type", "text/html", false);
  response.setStatusLine(request.httpVersion, "302", "Found");
  response.setHeader("Location", redirect, false);
  response.write(page);
}
