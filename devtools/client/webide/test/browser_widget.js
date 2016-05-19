/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

function test() {
  waitForExplicitFinish();
  Task.spawn(function* () {
    let win = yield openWebIDE();
    ok(document.querySelector("#webide-button"), "Found WebIDE button");
    Services.prefs.setBoolPref("devtools.webide.widget.enabled", false);
    ok(!document.querySelector("#webide-button"), "WebIDE button uninstalled");
    yield closeWebIDE(win);
    Services.prefs.clearUserPref("devtools.webide.widget.enabled");
  }).then(finish, handleError);
}
