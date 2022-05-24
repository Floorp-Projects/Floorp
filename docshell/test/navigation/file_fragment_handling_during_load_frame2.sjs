/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80 ft=javascript: */
"use strict";

function handleRequest(request, response) {
  response.setHeader("Content-Type", "text/html", false);
  response.setHeader("Cache-Control", "no-cache", false);
  // Wait a bit.
  var s = Date.now();
  // eslint-disable-next-line no-empty
  while (Date.now() - s < 1000) {}

  response.write(`<!DOCTYPE HTML>
  <html>
    <body>
     bar
    </body>
  </html>
  `);
}
