/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/test-bug-621644-jsterm-dollar.html";

add_task(function* () {
  yield loadTab(TEST_URI);

  let hud = yield openConsole();

  yield test$(hud);
  yield test$$(hud);
});

function* test$(HUD) {
  let deferred = promise.defer();

  HUD.jsterm.clearOutput();

  HUD.jsterm.execute("$(document.body)", (msg) => {
    ok(msg.textContent.indexOf("<p>") > -1,
       "jsterm output is correct for $()");
    deferred.resolve();
  });

  return deferred.promise;
}

function test$$(HUD) {
  let deferred = promise.defer();

  HUD.jsterm.clearOutput();

  HUD.jsterm.setInputValue();
  HUD.jsterm.execute("$$(document)", (msg) => {
    ok(msg.textContent.indexOf("621644") > -1,
       "jsterm output is correct for $$()");
    deferred.resolve();
  });

  return deferred.promise;
}
