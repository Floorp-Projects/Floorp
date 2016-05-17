/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that code completion works properly in chrome tabs, like about:credits.

"use strict";

function test() {
  Task.spawn(function* () {
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

    jsterm.complete(jsterm.COMPLETE_HINT_ONLY, function () {
      is(jsterm.completeNode.value, "    ment", "'docu' completion");
      deferred.resolve(null);
    });

    yield deferred.promise;
  }).then(finishTest);
}
