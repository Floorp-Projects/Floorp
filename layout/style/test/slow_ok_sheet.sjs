// Make sure our timer stays alive.
let gTimer;

function handleRequest(request, response) {
  response.setHeader("Content-Type", "text/css", false);
  response.setStatusLine("1.1", 200, "OK");
  response.processAsync();

  gTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  // Wait for 1s before responding; this should usually make sure this load comes in last.
  gTimer.init(
    () => {
      // This sheet _does_ still get applied even though its importing link
      // overall reports failure...
      response.write("nosuchelement { background: red }");
      response.finish();
    },
    1000,
    Ci.nsITimer.TYPE_ONE_SHOT
  );
}
