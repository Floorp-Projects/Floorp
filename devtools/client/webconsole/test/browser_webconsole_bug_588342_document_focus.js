/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = "data:text/html;charset=utf-8,Web Console test for bug 588342";

add_task(function* () {
  let { browser } = yield loadTab(TEST_URI);
  let hud = yield openConsole();

  yield checkConsoleFocus(hud);

  let isFocused = yield ContentTask.spawn(browser, { }, function* () {
    var fm = Components.classes["@mozilla.org/focus-manager;1"].
                         getService(Components.interfaces.nsIFocusManager);
    return fm.focusedWindow == content;
  });

  ok(isFocused, "content document has focus");
});

function* checkConsoleFocus(hud) {
  let fm = Cc["@mozilla.org/focus-manager;1"].getService(Ci.nsIFocusManager);

  yield new Promise(resolve => {
    waitForFocus(resolve);
  });

  is(hud.jsterm.inputNode.getAttribute("focused"), "true",
     "jsterm input is focused on web console open");
  is(fm.focusedWindow, hud.iframeWindow, "hud window is focused");
  yield closeConsole(null);
}
