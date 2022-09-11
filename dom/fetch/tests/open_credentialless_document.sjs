/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Created with: mktoken --origin 'https://example.com' --feature CoepCredentialless --expiry 'Wed, 01 Jan 3000 01:00:00 +0100' --sign test-keys/test-ecdsa.pkcs8
const TOKEN =
  "Az+DK2Kczk8Xz1cAlD+TkvPZmuM2uJZ2CFefbp2hLuCU9FbUqxWTyQ2tEYr50r0syKELcOZLAPaABw8aYTLHn5YAAABUeyJvcmlnaW4iOiJodHRwczovL2V4YW1wbGUuY29tIiwiZmVhdHVyZSI6IkNvZXBDcmVkZW50aWFsbGVzcyIsImV4cGlyeSI6MzI1MDM2ODAwMDB9";

function handleRequest(request, response) {
  let params = (request.queryString || "").split("&");
  if (params.includes("credentialless")) {
    response.setHeader("Cross-Origin-Embedder-Policy", "credentialless");
  } else if (params.includes("requirecorp")) {
    response.setHeader("Cross-Origin-Embedder-Policy", "require-corp");
  }
  let html = "<!doctype html>";
  if (params.includes("meta")) {
    response.setHeader("Origin-Trial", TOKEN);
  } else {
    html += `<meta http-equiv="origin-trial" content="${TOKEN}">`;
  }
  html += "<body>Hello, world!</body>";
  response.setHeader("Content-Type", "text/html;charset=utf-8", false);
  response.setStatusLine(request.httpVersion, "200", "Found");
  response.write(html);
}
