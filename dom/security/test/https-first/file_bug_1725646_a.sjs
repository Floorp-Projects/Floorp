"use strict";
/* eslint-disable @microsoft/sdl/no-insecure-url */

const URL_B =
  "http://example.com/tests/dom/security/test/https-first/file_bug_1725646_b.sjs";

const RESPONSE = `
<!DOCTYPE html>
<html>
<body>
<h1>Welcome to our insecure site!</h1>
<script type="application/javascript">
  window.opener.postMessage({location: location.href}, '*');
</script>
</body>
</html>`;

function handleRequest(request, response) {
  response.setHeader("Cache-Control", "no-cache", false);

  if (request.scheme === "http") {
    response.write(RESPONSE);
  } else {
    response.setStatusLine(request.httpVersion, 302, "Found");
    response.setHeader("Location", URL_B, false);
  }
}
