/*
 * TestSever customized specifically for the needs of:
 * Bug 1111834 - sendBeacon() should not follow 30x redirect after preflight
 *
 * Here is a sequence of the test:
 * [1] preflight channel (identified by the queryString 'beacon' and method 'OPTIONS')
 * [2] actual channel (identified by the queryString 'beacon') which gets redirected
 * [3] should never happen (the actual redirected request)
 * [4] xhr request (identified by the queryString 'verifyRedirectDidNotSucceed')
 *     which checks if the state was not changed from 'green' to 'red'. If the channel
 *     woulnd't be blocked correctly the redirected channel would set the state to 'red'.
 *
 */

function handleRequest(request, response)
{
  response.setHeader("Cache-Control", "no-cache, must-revalidate", false);

  // [Sequence 4]
  if (request.queryString === "verifyRedirectDidNotSucceed") {
    var redirectState = getState("redirectState");
    response.write(redirectState);
    return;
  }

  var originHeader = request.getHeader("origin");
  response.setHeader("Cache-Control", "no-cache, must-revalidate", false);
  response.setHeader("Access-Control-Allow-Headers", "content-type", false);
  response.setHeader("Access-Control-Allow-Methods", "POST, GET", false);
  response.setHeader("Access-Control-Allow-Origin", originHeader, false);
  response.setHeader("Access-Control-Allow-Credentials", "true", false);

  // [Sequence 1,2]
  if (request.queryString === "beacon") {
    setState("redirectState", "green");
    // [1]
    if (request.method == "OPTIONS") {
      response.setStatusLine(null, 200, "OK");
      return;
    }
    // [Sequence 2]
    var newLocation =
      "http://mochi.test:8888/tests/dom/tests/mochitest/beacon/beacon-cors-redirect-handler.sjs?redirected";
    response.setStatusLine("1.1", 302, "Found");
    response.setHeader("Location", newLocation, false);
    return;
  }

  // [Sequence 3]
  setState("redirectState", "red");
  response.setStatusLine(null, 200, "OK");
}
