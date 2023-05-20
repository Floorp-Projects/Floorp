function handleRequest(request, response) {
  response.processAsync();

  let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  timer.init(
    function () {
      response.finish();
    },
    5000,
    Ci.nsITimer.TYPE_ONE_SHOT
  );

  response.setStatusLine(null, 200, "OK");
  response.setHeader("Content-Type", "text/plain", false);
  response.write("Start of the content.");
}
