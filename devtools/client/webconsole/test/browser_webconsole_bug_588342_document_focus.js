/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TEST_URI = "data:text/html;charset=utf-8,Web Console test for bug 588342";

var fm = Cc["@mozilla.org/focus-manager;1"].getService(Ci.nsIFocusManager);

add_task(function* () {
  let { browser } = yield loadTab(TEST_URI);

  let hud = yield openConsole();
  yield consoleOpened(hud);

  is(fm.focusedWindow, browser.contentWindow,
     "content document has focus");

  fm = null;
});

function consoleOpened(hud) {
  let deferred = promise.defer();
  waitForFocus(function() {
    is(hud.jsterm.inputNode.getAttribute("focused"), "true",
       "jsterm input is focused on web console open");
    isnot(fm.focusedWindow, content, "content document has no focus");
    closeConsole(null).then(deferred.resolve);
  }, hud.iframeWindow);

  return deferred.promise;
}
