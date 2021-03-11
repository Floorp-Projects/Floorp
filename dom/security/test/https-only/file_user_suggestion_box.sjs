// Custom *.sjs file specifically for the needs of Bug 1665057
"use strict";

function handleRequest(request, response) {
  // avoid confusing cache behaviour
  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "text/html", false);

  // this can only be reached via clicking the button on the https-error page
  // to enable https-only mode temporarily
  if (request.scheme === "http") {
    response.write("This page is not secure!");
    return;
  }
  if (request.host.startsWith("www.")) {
    // in this test all pages that can be reached via https must have www.
    response.write("You are now on the secure www. page");
    createIframe();
    return;
  }
  // in this test there should not be a secure connection to a site without www.
  response.write("This page should not be reached");
  return;
}
