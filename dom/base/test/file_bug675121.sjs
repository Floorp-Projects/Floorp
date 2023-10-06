var timer;

function handleRequest(request, response) {
  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "text/plain", false);
  response.write("Responded");
  response.processAsync();
  timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  timer.initWithCallback(
    function () {
      response.finish();
      // 50ms certainly be enough for one refresh driver firing to happen!
    },
    50,
    Ci.nsITimer.TYPE_ONE_SHOT
  );
}
