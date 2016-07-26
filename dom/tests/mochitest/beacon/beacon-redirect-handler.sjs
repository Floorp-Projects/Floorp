/*
 * TestSever customized specifically for the needs of:
 * Bug 1280692 - sendBeacon() should follow 30x redirect
 *
 * Here is a sequence of the test:
 * [1] sendBeacon (identified by the queryString 'beacon') which gets redirected
 * [2] redirected sendBeacon (identified by the queryString 'redirected') which
 *     updates the state idniciating that redirected sendBeacon succeeds.
 * [3] xhr request (identified by the queryString 'verifyRedirectDidSucceed')
 *     which checks if the state was not changed from 'reset' to 'gree'. If the channel
 *     woulnd't be blocked correctly the redirected channel would set the state to 'red'.
 *
 */

function handleRequest(request, response)
{
  response.setHeader("Cache-Control", "no-cache, must-revalidate", false);

  // [Sequence 3]
  if (request.queryString === "verifyRedirectDidSucceed") {
    var redirectState = getState("redirectState");
    response.write(redirectState);
    return;
  }

  // [Sequence 1]
  if (request.queryString === "beacon") {
    setState("redirectState", "reset");
    var newLocation =
      "http://mochi.test:8888/tests/dom/tests/mochitest/beacon/beacon-redirect-handler.sjs?redirected";
    response.setStatusLine("1.1", 302, "Found");
    response.setHeader("Location", newLocation, false);
    return;
  }

  // [Sequence 2]
  if (request.queryString === "redirected") {
    setState("redirectState", "green");
    response.setStatusLine(null, 200, "OK");
    return;
  }

  // we should never get here, but just in case let's
  // set the state to something unexpected
  setState("redirectState", "red");
  response.setStatusLine(null, 200, "OK");
}
