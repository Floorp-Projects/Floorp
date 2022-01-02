// https://bugzilla.mozilla.org/show_bug.cgi?id=1613063

// Step 1. Send request with redirect queryString (eg. file_redirect.sjs?302)
// Step 2. Server responds with corresponding redirect code to http://example.com/../file_redirect.sjs?check
// Step 3. Response from ?check indicates whether the redirected request was secure or not.

const RESPONSE_SECURE = "secure-ok";
const RESPONSE_INSECURE = "secure-error";
const RESPONSE_ERROR = "unexpected-query";

function handleRequest(request, response) {
  response.setHeader("Cache-Control", "no-cache", false);

  const query = request.queryString;

  // Send redirect header
  if ((query >= 301 && query <= 303) || query == 307) {
    const loc =
      "http://example.com/tests/dom/security/test/https-only/file_redirect.sjs?check";
    response.setStatusLine(request.httpVersion, query, "Moved");
    response.setHeader("Location", loc, false);
    return;
  }

  // Check if scheme is http:// oder https://
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
