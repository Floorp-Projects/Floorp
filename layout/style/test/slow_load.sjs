// Make sure our timer stays alive.
let gTimer;

function handleRequest(request, response) {
  let isCss = request.queryString.indexOf("css") != -1;

  response.setHeader("Content-Type", isCss ? "text/css" : "text/plain", false);
  response.setStatusLine("1.1", 200, "OK");
  response.processAsync();

  let time = Date.now();

  gTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  // Wait for 1s before responding; this should usually make sure this load comes in last.
  gTimer.init(
    () => {
      if (isCss) {
        // FIXME(emilio): This clamps the date to the 32-bit integer range which
        // is what we use to store the z-index... We don't seem to store f64s
        // anywhere in the specified values...
        time = time % (Math.pow(2, 31) - 1);
        response.write(":root { z-index: " + time + "}");
      }
      response.finish();
    },
    1000,
    Ci.nsITimer.TYPE_ONE_SHOT
  );
}
