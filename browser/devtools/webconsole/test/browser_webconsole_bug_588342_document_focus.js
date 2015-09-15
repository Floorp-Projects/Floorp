/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *  Mihai Șucan <mihai.sucan@gmail.com>
 *
 * ***** END LICENSE BLOCK ***** */

"use strict";

const TEST_URI = "data:text/html;charset=utf-8,Web Console test for bug 588342";

var fm = Cc["@mozilla.org/focus-manager;1"].getService(Ci.nsIFocusManager);

var test = asyncTest(function* () {
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
