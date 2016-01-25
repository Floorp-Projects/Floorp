/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
