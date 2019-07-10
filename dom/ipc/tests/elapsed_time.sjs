/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const DELAY_MS = 200;

const HTML = `<!DOCTYPE HTML>
<html dir="ltr" xml:lang="en-US" lang="en-US">
  <head>
    <meta charset="utf8">
  </head>
  <body>
  </body>
</html>`;

/*
 * Keep timer as a global so that it is not GC'd
 * between the time that handleRequest() completes
 * and it expires.
 */
var timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
function handleRequest(req, resp) {
  resp.processAsync();
  resp.setHeader("Cache-Control", "no-cache", false);
  resp.setHeader("Content-Type", "text/html;charset=utf-8", false);

  resp.write(HTML);
  timer.init(() => {
    resp.write("");
    resp.finish();
  }, DELAY_MS, Ci.nsITimer.TYPE_ONE_SHOT);
}
