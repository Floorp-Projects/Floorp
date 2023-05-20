var timer = null;

function handleRequest(request, response) {
  response.processAsync();

  response.setStatusLine(null, 200, "OK");
  response.setHeader("Content-Type", "image/svg+xml", false);

  // We need some body output or else gecko will not do an initial reflow
  // while waiting for the rest of the document to load:
  response.bodyOutputStream.write("\n", 1);

  timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  timer.initWithCallback(
    function () {
      var body =
        "<svg xmlns='http://www.w3.org/2000/svg' width='70' height='0'></svg>";
      response.bodyOutputStream.write(body, body.length);
      response.finish();
    },
    1000 /* milliseconds */,
    Ci.nsITimer.TYPE_ONE_SHOT
  );
}
