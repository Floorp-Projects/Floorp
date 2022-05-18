"use strict";

let timer;

const DELAY_MS = 2000;
function handleRequest(request, response) {
  response.processAsync();
  timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  timer.init(
    () => {
      response.setHeader("Content-Type", "text/javascript", false);
      response.write("console.log('script loaded')\n");
      response.finish();
    },
    DELAY_MS,
    Ci.nsITimer.TYPE_ONE_SHOT
  );
}
