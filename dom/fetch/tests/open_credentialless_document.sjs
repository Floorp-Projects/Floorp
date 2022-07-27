/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const HTML = `<!DOCTYPE HTML>
<head>
  <!-- Created with: mktoken --origin 'https://example.com' --feature CoepCredentialless --expiry 'Wed, 01 Jan 3000 01:00:00 +0100' --sign test-keys/test-ecdsa.pkcs8 -->
  <meta http-equiv="origin-trial" content="Az+DK2Kczk8Xz1cAlD+TkvPZmuM2uJZ2CFefbp2hLuCU9FbUqxWTyQ2tEYr50r0syKELcOZLAPaABw8aYTLHn5YAAABUeyJvcmlnaW4iOiJodHRwczovL2V4YW1wbGUuY29tIiwiZmVhdHVyZSI6IkNvZXBDcmVkZW50aWFsbGVzcyIsImV4cGlyeSI6MzI1MDM2ODAwMDB9">
</head>
<html>
  <body>Hello World</body>
</html>`;

function handleRequest(request, response) {
  if (request.queryString == "credentialless") {
    response.setHeader("Cross-Origin-Embedder-Policy", "credentialless");
  } else if (request.queryString === "requirecorp") {
    response.setHeader("Cross-Origin-Embedder-Policy", "require-corp");
  }
  response.setHeader("Content-Type", "text/html;charset=utf-8", false);
  response.setStatusLine(request.httpVersion, "200", "Found");
  response.write(HTML);
}
