"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;

let timer;

const DELAY_MS = 5000;

function handleRequest(request, response) {
  response.processAsync();

  response.setHeader("Content-Type", "text/html", false);
  
  // Include paint_listener.js so that we can call waitForAllPaintsFlushed
  // on the window in which this is opened.
  response.write("<script type=\"text/javascript\" src=\"/tests/SimpleTest/paint_listener.js\"></script>");
  
  // Allow the opening window to react to loading being complete.
  response.write("<body onload=\"window.opener.fullyLoaded()\">");

  // Send half of the content.
  for (var i = 0; i < 100; ++i) {
    response.write("<p>Some text.</p>");
  }
  
  // Allow the opening window to react to being partially loaded.
  response.write("<script>window.opener.partiallyLoaded();</script>");

  // Wait for 5 seconds, then send the rest of the content.
  timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  timer.init(() => {
    
    for (var i = 0; i < 100; ++i) {
      response.write("<p>Some text.</p>");
    }
    response.write("</body>");
    
    response.finish();
  }, DELAY_MS, Ci.nsITimer.TYPE_ONE_SHOT);
}
