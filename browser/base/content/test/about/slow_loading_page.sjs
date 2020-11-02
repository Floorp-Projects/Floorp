/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const DELAY_MS = 400;

const HTML = `<!DOCTYPE HTML>
<html dir="ltr" xml:lang="en-US" lang="en-US">
  <head>
    <meta charset="utf8">
  </head>
  <body>hi mom!
  </body>
</html>`;

function handleRequest(req, resp) {
  resp.processAsync();

  let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  timer.init(
    () => {
      resp.setHeader("Cache-Control", "no-cache", false);
      resp.setHeader("Content-Type", "text/html;charset=utf-8", false);
      resp.write(HTML);
      resp.finish();
    },
    DELAY_MS,
    Ci.nsITimer.TYPE_ONE_SHOT
  );
}
