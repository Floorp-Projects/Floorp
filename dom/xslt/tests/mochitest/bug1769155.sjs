function waitForTrue(state) {
  return new Promise(resolve => {
    let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    timer.init(
      () => {
        if (getState(state) == "true") {
          timer.cancel();
          resolve();
        }
      },
      400,
      Ci.nsITimer.TYPE_REPEATING_SLACK
    );
  });
}
function handleRequest(request, response) {
  response.processAsync();

  if (request.queryString != "stop") {
    // This is called from a synchronous XHR that we want to block until
    // we get a stop notification.
    waitForTrue("stop").then(() => {
      response.write("");
      response.finish();

      // Signal the other connection that we've closed the connection
      // for the synchronous XHR.
      setState("stopped", "true");
    });
  } else {
    // Close the connection for the synchronous XHR.
    setState("stop", "true");

    // Let's wait until we've actually closed the connection for the XHR.
    waitForTrue("stopped").then(() => {
      response.write("");
      response.finish();
    });
  }
}
