function handleRequest(request, response) {
  response.processAsync();

  timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  timer.init(
    function () {
      response.write("Here the content. But slowly.");
      response.finish();
    },
    1000,
    Ci.nsITimer.TYPE_ONE_SHOT
  );
}
