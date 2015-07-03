/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *  Julian Viereck <jviereck@mozilla.com>
 *  Patrick Walton <pcwalton@mozilla.com>
 *  Mihai Șucan <mihai.sucan@gmail.com>
 *
 * ***** END LICENSE BLOCK ***** */

// Tests that, when the user types an extraneous closing bracket, no error
// appears.

"use strict";

const TEST_URI = "data:text/html;charset=utf-8,test for bug 592442";

let test = asyncTest(function*() {
  yield loadTab(TEST_URI);
  let hud = yield openConsole();
  hud.jsterm.clearOutput();
  let jsterm = hud.jsterm;

  jsterm.setInputValue("document.getElementById)");

  let error = false;
  try {
    jsterm.complete(jsterm.COMPLETE_HINT_ONLY);
  } catch (ex) {
    error = true;
  }

  ok(!error, "no error was thrown when an extraneous bracket was inserted");
});
