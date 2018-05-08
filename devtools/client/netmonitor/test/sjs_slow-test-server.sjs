"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;

let timer;

const DELAY_MS = 10000;
function handleRequest(request, response) {
  response.processAsync();
  timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  timer.init(() => {
    response.setHeader("Content-Type", "text/html", false);
    response.write("<body>Slow loading page for netmonitor test. You should never see this.</body>");
    response.finish();
  }, DELAY_MS, Ci.nsITimer.TYPE_ONE_SHOT);
}
