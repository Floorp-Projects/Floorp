// custom *.sjs file for
// Bug 1402363: Test Mixed Content Redirect Blocking.

const URL_PATH = "example.com/tests/dom/security/test/mixedcontentblocker/";

function handleRequest(request, response) {
  response.setHeader("Cache-Control", "no-cache", false);
  let queryStr = request.queryString;

  if (queryStr === "https-to-https-redirect") {
    response.setStatusLine("1.1", 302, "Found");
    response.setHeader(
      "Location",
      "https://" + URL_PATH + "file_redirect_handler.sjs?load",
      false
    );
    return;
  }

  if (queryStr === "https-to-http-redirect") {
    response.setStatusLine("1.1", 302, "Found");
    response.setHeader(
      "Location",
      "http://" + URL_PATH + "file_redirect_handler.sjs?load",
      false
    );
    return;
  }

  if (queryStr === "load") {
    response.setHeader("Content-Type", "text/html", false);
    response.write("foo");
  }
}
