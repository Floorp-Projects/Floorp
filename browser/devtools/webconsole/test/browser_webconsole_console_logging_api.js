/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is DevTools test code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  David Dahl <ddahl@mozilla.com>
 *  Patrick Walton <pcwalton@mozilla.com>
 *  Julian Viereck <jviereck@mozilla.com>
 *  Mihai Sucan <mihai.sucan@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// Tests that the basic console.log()-style APIs and filtering work.

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-console.html";

let testDriver = null;
let subtestDriver = null;

function test() {
  addTab(TEST_URI);

  browser.addEventListener("DOMContentLoaded", onLoad, false);
}

function onLoad() {
  browser.removeEventListener("DOMContentLoaded", onLoad, false);

  openConsole(null, function(aHud) {
    hud = aHud;
    hudId = hud.hudId;
    outputNode = hud.outputNode;
    testDriver = testGen();
    testDriver.next();
  });
}

function testGen() {
  subtestGen("log");
  yield;

  subtestGen("info");
  yield;

  subtestGen("warn");
  yield;

  subtestGen("error");
  yield;

  subtestGen("debug"); // bug 616742
  yield;

  testDriver = subtestDriver = null;
  finishTest();

  yield;
}

function subtestGen(aMethod) {
  subtestDriver = testConsoleLoggingAPI(aMethod);
  subtestDriver.next();
}

function testConsoleLoggingAPI(aMethod) {
  let console = content.wrappedJSObject.console;

  hud.jsterm.clearOutput();

  setStringFilter(hudId, "foo");
  console[aMethod]("foo-bar-baz");
  console[aMethod]("bar-baz");

  function nextTest() {
    subtestDriver.next();
  }

  waitForSuccess({
    name: "1 hidden " + aMethod + " node via string filtering",
    validatorFn: function()
    {
      return outputNode.querySelectorAll(".hud-filtered-by-string").length == 1;
    },
    successFn: nextTest,
    failureFn: nextTest,
  });

  yield;

  hud.jsterm.clearOutput();

  // now toggle the current method off - make sure no visible message

  // TODO: move all filtering tests into a separate test file: see bug 608135
  setStringFilter(hudId, "");
  HUDService.setFilterState(hudId, aMethod, false);
  console[aMethod]("foo-bar-baz");

  waitForSuccess({
    name: "1 message hidden for " + aMethod + " (logging turned off)",
    validatorFn: function()
    {
      return outputNode.querySelectorAll("description").length == 1;
    },
    successFn: nextTest,
    failureFn: nextTest,
  });

  yield;

  hud.jsterm.clearOutput();
  HUDService.setFilterState(hudId, aMethod, true);
  console[aMethod]("foo-bar-baz");

  waitForSuccess({
    name: "1 message shown for " + aMethod + " (logging turned on)",
    validatorFn: function()
    {
      return outputNode.querySelectorAll("description").length == 1;
    },
    successFn: nextTest,
    failureFn: nextTest,
  });

  yield;

  hud.jsterm.clearOutput();
  setStringFilter(hudId, "");

  // test for multiple arguments.
  console[aMethod]("foo", "bar");

  waitForSuccess({
    name: "show both console arguments for " + aMethod,
    validatorFn: function()
    {
      let node = outputNode.querySelector(".hud-msg-node");
      return node && /foo bar/.test(node.textContent);
    },
    successFn: nextTest,
    failureFn: nextTest,
  });

  yield;
  testDriver.next();
  yield;
}

function setStringFilter(aId, aValue) {
  hud.filterBox.value = aValue;
  HUDService.adjustVisibilityOnSearchStringChange(aId, aValue);
}

