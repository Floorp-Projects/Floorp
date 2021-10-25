/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

function handleRequest(request, response) {
  response.setStatusLine(request.httpVersion, 200, "Och Aye");

  response.setHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  response.setHeader("Pragma", "no-cache");
  response.setHeader("Expires", "0");
  response.setHeader("Content-Type", "text/plain; charset=utf-8", false);

  // Taken from devtools/shared/webconsole/network-monitor
  const NETMONITOR_LIMIT = 1048576;

  // 2 * NETMONITOR_LIMIT reaches the exact limit for the netmonitor
  // 3 * NETMONITOR_LIMIT makes sure we go past it.
  response.write("x".repeat(3 * NETMONITOR_LIMIT));
}
