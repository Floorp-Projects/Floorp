"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;

let timer;

const DELAY_MS = 5000;
function handleRequest(request, response) {
  if (request.queryString.endsWith("faster")) {
    response.setHeader("Content-Type", "text/html", false);
    response.write("<body>Not so slow!</body>");
    return;
  }
  response.processAsync();
  timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  timer.init(() => {
    response.setHeader("Content-Type", "text/html", false);
    response.write("<body>This was the slow load. You should never see this.</body>");
    response.finish();
  }, DELAY_MS, Ci.nsITimer.TYPE_ONE_SHOT);
}
