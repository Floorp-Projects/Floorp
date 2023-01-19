/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80 ft=javascript: */
"use strict";

const REDIRECT_TO =
  "http://mochi.test:8888/tests/docshell/test/navigation/file_reload_postdata_redirect.sjs?redirected";

function handleRequest(request, response) {
  if (!request.queryString) {
    response.setHeader("Content-Type", "text/html", false);
    response.setStatusLine(request.httpVersion, "307", "Found");
    response.setHeader("Location", REDIRECT_TO, false);
    response.write("redirect");
  } else {
    response.setHeader("Content-Type", "text/html", false);
    response.setHeader("Cache-Control", "no-cache", false);
    response.write(`
      <!DOCTYPE HTML>
      <script> opener.postMessage("${request.method}", "*");</script>
    `);
  }
}
