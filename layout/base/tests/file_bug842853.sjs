var timer;

function handleRequest(request, response) {
  response.setHeader("Cache-Control", "no-cache, must-revalidate", false);
  response.setHeader("Content-Type", "text/css", false);
  response.write("body { background:lime; color:red; }");
  response.processAsync();
  timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  timer.initWithCallback(
    function () {
      response.finish();
    },
    500,
    Ci.nsITimer.TYPE_ONE_SHOT
  );
}
