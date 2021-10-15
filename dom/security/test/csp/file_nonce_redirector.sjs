// custom *.sjs file for
// Bug 1469150:Scripts with valid nonce get blocked if URL redirects.

const URL_PATH = "example.com/tests/dom/security/test/csp/";

function handleRequest(request, response) {
  response.setHeader("Cache-Control", "no-cache", false);
  let queryStr = request.queryString;

  if (queryStr === "redirect") {
    response.setStatusLine("1.1", 302, "Found");
    response.setHeader("Location",
      "https://" + URL_PATH + "file_nonce_redirector.sjs?load", false);
    return;
  }

  if (queryStr === "load") {
    response.setHeader("Content-Type", "application/javascript", false);
    response.write("console.log('script loaded');");
    return;
  }

  // we should never get here - return something unexpected
  response.write("d'oh");
}
