"use strict";

// redirection uri
const REDIRECT_URI =
  "https://example.com/tests/dom/security/test/https-first/file_multiple_redirection.sjs?redirect";
const REDIRECT_URI_HTTP =
  "http://example.com/tests/dom/security/test/https-first/file_multiple_redirection.sjs?verify";
const REDIRECT_URI_HTTPS =
  "https://example.com/tests/dom/security/test/https-first/file_multiple_redirection.sjs?verify";

const RESPONSE_ERROR = "unexpected-query";

// An onload postmessage to window opener
const RESPONSE_HTTPS_SCHEME = `
  <html>
  <body>
  <script type="application/javascript">
    window.opener.postMessage({result: 'scheme-https'}, '*');
  </script>
  </body>
  </html>`;

const RESPONSE_HTTP_SCHEME = `
  <html>
  <body>
  <script type="application/javascript">
    window.opener.postMessage({result: 'scheme-http'}, '*');
  </script>
  </body>
  </html>`;

function sendRedirection(query, response) {
  // send a redirection to an http uri
  if (query.includes("test1")) {
    response.setHeader("Location", REDIRECT_URI_HTTP, false);
    return;
  }
  // send a redirection to an https uri
  if (query.includes("test2")) {
    response.setHeader("Location", REDIRECT_URI_HTTPS, false);
    return;
  }
  // send a redirection to an http uri with hsts header
  if (query.includes("test3")) {
    response.setHeader("Strict-Transport-Security", "max-age=60");
    response.setHeader("Location", REDIRECT_URI_HTTP, false);
  }
}

function handleRequest(request, response) {
  response.setHeader("Cache-Control", "no-cache", false);
  const query = request.queryString;

  // if the query contains a test query start first test
  if (query.startsWith("test")) {
    // send a 302 redirection
    response.setStatusLine(request.httpVersion, 302, "Found");
    response.setHeader("Location", REDIRECT_URI + query, false);
    return;
  }
  // Send a redirection
  if (query.includes("redirect")) {
    response.setStatusLine(request.httpVersion, 302, "Found");
    sendRedirection(query, response);
    return;
  }
  // Reset the HSTS policy, prevent influencing other tests
  if (request.queryString === "reset") {
    response.setHeader("Strict-Transport-Security", "max-age=0");
    let response_content =
      request.scheme === "https" ? RESPONSE_HTTPS_SCHEME : RESPONSE_HTTP_SCHEME;
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.write(response_content);
  }
  // Check if scheme is http:// or https://
  if (query == "verify") {
    let response_content =
      request.scheme === "https" ? RESPONSE_HTTPS_SCHEME : RESPONSE_HTTP_SCHEME;
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.write(response_content);
    return;
  }

  // We should never get here, but just in case ...
  response.setStatusLine(request.httpVersion, 500, "OK");
  response.write("unexepcted query");
}
