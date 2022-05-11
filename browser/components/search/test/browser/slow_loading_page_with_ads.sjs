/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function handleRequest(request, response) {
  const DELAY_MS = 2000;
  response.processAsync();

  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "image/png", false);
  response.write("Start loading image");

  let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  timer.init(
    () => {
      response.write("Finish loading image");
      response.finish();
    },
    DELAY_MS,
    Ci.nsITimer.TYPE_ONE_SHOT
  );
}
