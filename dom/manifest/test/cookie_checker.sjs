"use strict";
let { NetUtil } = ChromeUtils.importESModule(
  "resource://gre/modules/NetUtil.sys.mjs"
);

function handleRequest(request, response) {
  response.setStatusLine(request.httpVersion, 200);
  // avoid confusing cache behaviors
  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "application/json", false);

  // CORS stuff
  const origin = request.hasHeader("Origin")
    ? request.getHeader("Origin")
    : null;
  if (origin) {
    response.setHeader("Access-Control-Allow-Origin", origin);
    response.setHeader("Access-Control-Allow-Credentials", "true");
  }
  const short_name = request.hasHeader("Cookie")
    ? request.getHeader("Cookie")
    : "no cookie";
  response.write(JSON.stringify({ short_name }));
}
