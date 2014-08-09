/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests that code completion works properly in chrome tabs, like about:credits.

"use strict";

function test() {
  Task.spawn(function*() {
    const {tab} = yield loadTab("about:config");
    ok(tab, "tab loaded");

    const hud = yield openConsole(tab);
    ok(hud, "we have a console");
    ok(hud.iframeWindow, "we have the console UI window");

    let jsterm = hud.jsterm;
    ok(jsterm, "we have a jsterm");

    let input = jsterm.inputNode;
    ok(hud.outputNode, "we have an output node");

    // Test typing 'docu'.
    input.value = "docu";
    input.setSelectionRange(4, 4);

    let deferred = promise.defer();

    jsterm.complete(jsterm.COMPLETE_HINT_ONLY, function() {
      is(jsterm.completeNode.value, "    ment", "'docu' completion");
      deferred.resolve(null);
    });

    yield deferred.promise;
  }).then(finishTest);
}
