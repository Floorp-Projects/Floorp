/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var timer = Components.classes["@mozilla.org/timer;1"];
var waitTimer = timer.createInstance(Components.interfaces.nsITimer);

function handleRequest(request, response) {
  response.setHeader("Content-Type", "text/html", false);
  response.setHeader("Cache-Control", "no-cache", false);
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.processAsync();
  waitForFinish(response);
}

function waitForFinish(response) {
  if (getSharedState("all-parts-done") === "1") {
    response.write("done");
    response.finish();
  } else {
    waitTimer.initWithCallback(function() {waitForFinish(response);}, 10,
                               Components.interfaces.nsITimer.TYPE_ONE_SHOT);
  }
}
