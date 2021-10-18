// Make sure our timer stays alive.
let gTimer;

function handleRequest(request, response) {
  response.setHeader("Content-Type", "text/html", false);
  response.setStatusLine("1.1", 404, "Not Found");
  response.processAsync();

  gTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  // Wait for 1s before responding; this should usually make sure this load comes in last.
  gTimer.init(
    () => {
      response.write("<h1>Hello</h1>");
      response.finish();
    },
    1000,
    Ci.nsITimer.TYPE_ONE_SHOT
  );
}
