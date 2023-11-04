/**
 * Custom *.sjs specifically for the needs of
 * Bug 921493 - CSP: test allowlisting of scheme-relative sources
 */

function handleRequest(request, response) {
  let query = new URLSearchParams(request.queryString);

  let scheme = query.get("scheme");
  let policy = query.get("policy");

  let linkUrl =
    scheme +
    "://example.com/tests/dom/security/test/csp/file_scheme_relative_sources.js";

  let html =
    "<!DOCTYPE HTML>" +
    "<html>" +
    "<head>" +
    "<title>test schemeless sources within CSP</title>" +
    "</head>" +
    "<body> " +
    "<div id='testdiv'>blocked</div>" +
    // try to load a scheme relative script
    "<script src='" +
    linkUrl +
    "'></script>" +
    // have an inline script that reports back to the parent whether
    // the script got loaded or not from within the sandboxed iframe.
    "<script type='application/javascript'>" +
    "window.onload = function() {" +
    "var inner = document.getElementById('testdiv').innerHTML;" +
    "window.parent.postMessage({ result: inner }, '*');" +
    "}" +
    "</script>" +
    "</body>" +
    "</html>";

  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "text/html", false);
  response.setHeader("Content-Security-Policy", policy, false);

  response.write(html);
}
