"use strict";
/* eslint-disable @microsoft/sdl/no-insecure-url */

const URL_A =
  "http://example.com/tests/dom/security/test/https-first/file_bug_1725646_a.sjs";
const URL_B =
  "http://example.com/tests/dom/security/test/https-first/file_bug_1725646_b.sjs";

const RESPONSE = `
<!DOCTYPE html>
<html>
<body>
<h1>We don't support HTTPS :(</h1>
<p>You will be redirected</p>
<script type="application/javascript">
  window.opener.postMessage({ location: location.href }, "*");
  setTimeout(() => {
    window.location = "${URL_A}";
  });
</script>
</body>
</html>
`;

function handleRequest(request, response) {
  response.setHeader("Cache-Control", "no-cache", false);

  if (request.scheme === "http") {
    response.write(RESPONSE);
  } else {
    response.setStatusLine(request.httpVersion, 302, "Found");
    response.setHeader("Location", URL_B, false);
  }
}
