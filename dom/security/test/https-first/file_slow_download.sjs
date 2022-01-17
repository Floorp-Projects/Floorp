"use strict";
let timer;

// Send a part of the file then wait for 3 second before sending the rest.
// If download isn't exempt from background timer of https-only/-first then the download
// gets cancelled before it completed.
const DELAY_MS = 3500;
function handleRequest(request, response) {
  response.processAsync();
  timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader(
    "Content-Disposition",
    "attachment; filename=large-dummy-file.txt"
  );
  response.setHeader("Content-Type", "text/plain");
  response.write("Begin the file");
  timer.init(
    () => {
      response.write("End of file");
      response.finish();
    },
    DELAY_MS,
    Ci.nsITimer.TYPE_ONE_SHOT
  );
}
