"use strict";

const FINAL_DOCUMENT = `
  <html>
  <body>
  final document
  <script>
  window.onload = function() {
    let docURI =  document.documentURI;
    window.opener.parent.postMessage({docURI}, "*");
    window.close();
  }
  </script>
  </body>
  </html>`;

function handleRequest(request, response) {
  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "text/html", false);

  const query = request.queryString;

  if (query === "same_origin_redirect") {
    let newLocation =
      "http://example.com/tests/dom/security/test/csp/file_upgrade_insecure_navigation_redirect.sjs?finaldoc_same_origin_redirect";
    response.setStatusLine("1.1", 302, "Found");
    response.setHeader("Location", newLocation, false);
    return;
  }

  if (query === "cross_origin_redirect") {
    let newLocation =
      "http://test1.example.com/tests/dom/security/test/csp/file_upgrade_insecure_navigation_redirect.sjs?finaldoc_cross_origin_redirect";
    response.setStatusLine("1.1", 302, "Found");
    response.setHeader("Location", newLocation, false);
    return;
  }

  if (
    query === "finaldoc_same_origin_redirect" ||
    query === "finaldoc_cross_origin_redirect"
  ) {
    response.write(FINAL_DOCUMENT);
    return;
  }

  // we should never get here, but just in case
  // return something unexpected
  response.write("do'h");
}
