"use strict";

const REDIRECT_URI = "http://example.com/tests/dom/security/test/https-first/file_break_endless_upgrade_downgrade_loop.sjs?verify";

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

function handleRequest(request, response) {
  response.setHeader("Cache-Control", "no-cache", false);
  const query = request.queryString;

  // handle the redirect case
  if ((query >= 301 && query <= 303) || query == 307) {
    // send same-origin downgrade from https: to http: again simluating
    // and endless upgrade downgrade loop.
    response.setStatusLine(request.httpVersion, query, "Found");
    response.setHeader("Location", REDIRECT_URI, false);
    return;
  }

  // Check if scheme is http:// or https://
  if (query == "verify") {
    let response_content = request.scheme === "https"
      ? RESPONSE_HTTPS_SCHEME
      : RESPONSE_HTTP_SCHEME;
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.write(response_content);
    return;
  }

  // We should never get here, but just in case ...
  response.setStatusLine(request.httpVersion, 500, "OK");
  response.write("unexepcted query");
}
