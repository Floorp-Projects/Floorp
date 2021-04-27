//https://bugzilla.mozilla.org/show_bug.cgi?id=1706351

// Step 1. Send request with redirect queryString (eg. file_redirect.sjs?302)
// Step 2. Server responds with corresponding redirect code to http://example.com/../file_redirect.sjs?check
// Step 3. Response from ?check indicates whether the redirected request was secure or not.

const RESPONSE_ERROR = "unexpected-query";

// An onload postmessage to window opener
const RESPONSE_SECURE = `
  <html>
  <body>
  send onload message...
  <script type="application/javascript">
    window.opener.postMessage({result: 'secure'}, '*');
  </script>
  </body>
  </html>`;

const RESPONSE_INSECURE = `
  <html>
  <body>
  send onload message...
  <script type="application/javascript">
    window.opener.postMessage({result: 'insecure'}, '*');
  </script>
  </body>
  </html>`;

function handleRequest(request, response) {
  response.setHeader("Cache-Control", "no-cache", false);

  const query = request.queryString;
  // Send redirect header
  if ((query >= 301 && query <= 303) || query == 307) {
    const loc =
      "http://example.com/tests/dom/security/test/https-first/file_redirect.sjs?check";
    response.setStatusLine(request.httpVersion, query, "Found");
    response.setHeader("Location", loc, false);
    return;
  }

  // Check if scheme is http:// or https://
  if (query == "check") {
    const secure =
      request.scheme == "https" ? RESPONSE_SECURE : RESPONSE_INSECURE;
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.write(secure);
    return;
  }

  // This should not happen
  response.setStatusLine(request.httpVersion, 500, "OK");
  response.write(RESPONSE_ERROR);
}
