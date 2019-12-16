/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function handleRequest(request, response) {
  response.processAsync();

  let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  timer.initWithCallback(() => {
    // to avoid garbage collection
    timer = null;
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.setHeader("Content-Type", "text/plain", false);
    response.setHeader("Set-Cookie", "foo=bar; Max-Age=10; HttpOnly", true);
    response.write("Some response data");
    response.finish();
  }, 300, Ci.nsITimer.TYPE_ONE_SHOT); // Make sure this request takes a few hundred ms.
}
