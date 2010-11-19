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

const TEST_URI = "http://example.com/browser/dom/tests/browser/test-console-api.html";

var gWindow;

function test() {
  // FIXME
  todo(false, "This test is disabled until bug 587734 lands");
  return;

  waitForExplicitFinish();

  var tab = gBrowser.addTab(TEST_URI);
  gBrowser.selectedTab = tab;
  var browser = gBrowser.selectedBrowser;

  registerCleanupFunction(function () {
    gBrowser.removeTab(tab);
  });

  ConsoleObserver.init();

  browser.addEventListener("DOMContentLoaded", function onLoad(event) {
    browser.removeEventListener("DOMContentLoaded", onLoad, false);
    executeSoon(function test_executeSoon() {
      gWindow = browser.contentWindow;
      consoleAPISanityTest();
      observeConsoleTest();
    });

  }, false);
}

var gWindow;

function testConsoleData(aMessageObject) {
  let messageWindow = getWindowByWindowId(aMessageObject.ID);
  is(messageWindow, gWindow, "found correct window by window ID");

  is(aMessageObject.level, gLevel, "expected level received");
  ok(aMessageObject.arguments, "we have arguments");
  is(aMessageObject.arguments.length, gArgs.length, "arguments.length matches");
  gArgs.forEach(function (a, i) {
    is(aMessageObject.arguments[i], a, "correct arg " + i);
  });

  if (aMessageObject.level == "error") {
    // Test finished
    ConsoleObserver.destroy();
    finish();
  }
}

var gLevel, gArgs;
function expect(level) {
  gLevel = level;
  gArgs = Array.slice(arguments, 1);
}

function observeConsoleTest() {
  let win = XPCNativeWrapper.unwrap(gWindow);
  expect("log", "arg");
  win.console.log("arg");

  expect("info", "arg", "extra arg");
  win.console.info("arg", "extra arg");

  expect("warn", "arg", "extra arg", 1);
  win.console.warn("arg", "extra arg", 1);

  expect("error", "arg");
  win.console.error("arg");
}

function consoleAPISanityTest() {
  let win = XPCNativeWrapper.unwrap(gWindow);
  ok(win.console, "we have a console attached");
  ok(win.console, "we have a console attached, 2nd attempt");

  ok(win.console.log, "console.log is here");
  ok(win.console.info, "console.info is here");
  ok(win.console.warn, "console.warn is here");
  ok(win.console.error, "console.error is here");
}

var ConsoleObserver = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),

  init: function CO_init() {
    Services.obs.addObserver(this, "console-api-log-event", false);
  },

  destroy: function CO_destroy() {
    Services.obs.removeObserver(this, "console-api-log-event");
  },

  observe: function CO_observe(aSubject, aTopic, aData) {
    try {
      testConsoleData(aSubject.wrappedJSObject);
    } catch (ex) {
      // XXX Exceptions in this function currently aren't reported, because of
      // some XPConnect weirdness, so report them manually
      ok(false, "Exception thrown in CO_observe: " + ex);
    }
  }
};

function getWindowId(aWindow)
{
  return aWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                .getInterface(Ci.nsIDOMWindowUtils)
                .outerWindowID;
}

function getWindowByWindowId(aId) {
  let someWindow = Services.wm.getMostRecentWindow("navigator:browser");
  if (someWindow) {
    let windowUtils = someWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                                .getInterface(Ci.nsIDOMWindowUtils);
    return windowUtils.getOuterWindowWithId(aId);
  }
  return null;
}
