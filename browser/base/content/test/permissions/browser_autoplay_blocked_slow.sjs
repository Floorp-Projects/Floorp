/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const DELAY_MS = "1000";

const AUTOPLAY_HTML = `<!DOCTYPE HTML>
<html dir="ltr" xml:lang="en-US" lang="en-US">
  <head>
    <meta charset="utf8">
  </head>
  <body>
    <audio autoplay="autoplay" >
      <source src="audio.ogg" />
    </audio>
    <script>setTimeout(() => { document.location.href = '#foo'; }, 500);</script>
  </body>
</html>`;

function handleRequest(req, resp) {
  resp.processAsync();
  resp.setHeader("Cache-Control", "no-cache", false);
  resp.setHeader("Content-Type", "text/html;charset=utf-8", false);

  let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  resp.write(AUTOPLAY_HTML);
  timer.init(() => {
    resp.write("");
    resp.finish();
  }, DELAY_MS, Ci.nsITimer.TYPE_ONE_SHOT);
}