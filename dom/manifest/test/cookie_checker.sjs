"use strict";
Components.utils.import("resource://gre/modules/NetUtil.jsm");

function handleRequest(request, response) {
  response.setStatusLine(request.httpVersion, 200);
  // avoid confusing cache behaviors
  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "application/json", false);
  const short_name = request.hasHeader("Cookie")
    ? `FAIL - cookie was present: ${request.getHeader("Cookie")}`
    : "PASS";
  response.write(JSON.stringify({ short_name }));
}
