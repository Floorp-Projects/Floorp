/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

async function handleRequest(request, response) {
  response.processAsync();

  const timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  timer.init(response.finish, 3000, Ci.nsITimer.TYPE_ONE_SHOT);
}
