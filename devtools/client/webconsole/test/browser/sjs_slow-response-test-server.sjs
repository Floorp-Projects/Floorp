/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

function handleRequest(request, response) {
  response.processAsync();

  const params = new Map(
    request.queryString
      .replace("?", "")
      .split("&")
      .map(s => s.split("="))
  );
  const delay = params.has("delay") ? params.get("delay") : 300;
  const status = params.has("status") ? params.get("status") : 200;

  let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  timer.initWithCallback(
    () => {
      // to avoid garbage collection
      timer = null;
      response.setStatusLine(request.httpVersion, status, "OK");
      response.setHeader("Content-Type", "text/plain", false);
      response.setHeader(
        "Set-Cookie",
        "foo=bar; Max-Age=10; HttpOnly; SameSite=Lax",
        true
      );
      response.write("Some response data");
      response.finish();
    },
    delay,
    Ci.nsITimer.TYPE_ONE_SHOT
  );
}
