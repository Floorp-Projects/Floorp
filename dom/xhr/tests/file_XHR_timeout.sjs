var timer = null;

function handleRequest(request, response) {
  response.processAsync();
  timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  timer.initWithCallback(
    function () {
      response.setStatusLine(null, 200, "OK");
      response.setHeader("Content-Type", "text/plain", false);
      response.write("hello");
      response.finish();
    },
    2500 /* milliseconds */,
    Ci.nsITimer.TYPE_ONE_SHOT
  );
}
