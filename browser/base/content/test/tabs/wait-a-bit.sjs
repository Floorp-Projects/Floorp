/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// eslint-disable-next-line mozilla/no-redeclare-with-import-autofix
const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs"
);

async function handleRequest(request, response) {
  response.seizePower();

  await new Promise(r => setTimeout(r, 2000));

  response.write("HTTP/1.1 200 OK\r\n");
  const body = "<title>wait a bit</title><body>ok</body>";
  response.write("Content-Type: text/html\r\n");
  response.write(`Content-Length: ${body.length}\r\n`);
  response.write("\r\n");
  response.write(body);
  response.finish();
}
