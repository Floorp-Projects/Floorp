/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *  Mihai Șucan <mihai.sucan@gmail.com>
 *
 * ***** END LICENSE BLOCK ***** */

const TEST_URI = "data:text/html;charset=utf-8,Web Console test for bug 588342";
let fm;

function test() {
  fm = Cc["@mozilla.org/focus-manager;1"].getService(Ci.nsIFocusManager);
  addTab(TEST_URI);
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    openConsole(null, consoleOpened);
  }, true);
}

function consoleOpened(hud) {
  waitForFocus(function() {
    is(hud.jsterm.inputNode.getAttribute("focused"), "true",
       "jsterm input is focused on web console open");
    isnot(fm.focusedWindow, content, "content document has no focus");
    closeConsole(null, consoleClosed);
  }, hud.iframeWindow);
}

function consoleClosed() {
  is(fm.focusedWindow, browser.contentWindow,
     "content document has focus");

  fm = null;
  finishTest();
}

