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
 * The Original Code is bug 556061 test.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Paul O’Shannessy <paul@oshannessy.com>
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

let cbSvc = Cc["@mozilla.org/widget/clipboard;1"].
            getService(Ci.nsIClipboard);

let testURL = "http://example.org/browser/browser/base/content/test/dummy_page.html";
let testActionURL = "moz-action:switchtab," + testURL;
let testTab;
let clipboardText = "";
let currentClipboardText = null;
let clipboardPolls = 0;

// The clipboard can have a string value without it being the one we expect, so
// we'll check the current value against the previous value to see if it changed.
// We can do this because our expected clipboard value should be different each
// time we wait.
function waitForClipboard() {
  // Poll for a maximum of 5s (each run happens after 100ms).
  if (++clipboardPolls > 50) {
    // Log the failure.
    ok(false, "Timed out while polling clipboard for pasted data");
    // Cleanup and interrupt the test.
    cleanup();
    return;
  }

  let xferable = Cc["@mozilla.org/widget/transferable;1"].
                 createInstance(Ci.nsITransferable);
  xferable.addDataFlavor("text/unicode");
  cbSvc.getData(xferable, cbSvc.kGlobalClipboard);
  try {
    let data = {};
    xferable.getTransferData("text/unicode", data, {});
    currentClipboardText = data.value.QueryInterface(Ci.nsISupportsString).data;
  } catch (e) {}

  if (currentClipboardText == clipboardText) {
    setTimeout(waitForClipboard, 100);
  } else {
    clipboardText = currentClipboardText;
    runNextTest();
  }
}

function runNextTest() {
  // reset clipboard polling count
  clipboardPolls = 0;
  // run next test, just assume we won't call in here without more tests
  tests.shift()();
}

function cleanup() {
  gBrowser.removeTab(testTab);
  finish();
}

// Tests in order. Some tests setup for the next actual test...
let tests = [
  function () {
    // Set the urlbar to include the moz-action
    gURLBar.value = testActionURL;
    is(gURLBar.value, testActionURL, "gURLBar.value starts with correct value");

    // Focus the urlbar so we can select it all & copy
    gURLBar.focus();
    gURLBar.select();
    goDoCommand("cmd_copy");
    waitForClipboard();
  },
  function () {
    is(clipboardText, testURL, "Clipboard has the correct value");
    // We shouldn't have changed the value of gURLBar
    is(gURLBar.value, testActionURL, "gURLBar.value didn't change when copying");

    // Set selectionStart/End manually and make sure it matches the substring
    gURLBar.selectionStart = 0;
    gURLBar.selectionEnd = 10;
    goDoCommand("cmd_copy");
    waitForClipboard();
  },
  function () {
    is(clipboardText, testURL.substring(0, 10), "Clipboard has the correct value");
    is(gURLBar.value, testActionURL, "gURLBar.value didn't change when copying");

    // Setup for cut test...
    // Select all
    gURLBar.select();
    goDoCommand("cmd_cut");
    waitForClipboard();
  },
  function () {
    is(clipboardText, testURL, "Clipboard has the correct value");
    is(gURLBar.value, "", "gURLBar.value is now empty");

    // Reset urlbar value
    gURLBar.value = testActionURL;
    // Sanity check that we have the right value
    is(gURLBar.value, testActionURL, "gURLBar.value starts with correct value");

    // Now just select part of the value & cut that.
    gURLBar.selectionStart = testURL.length - 10;
    gURLBar.selectionEnd = testURL.length;

    goDoCommand("cmd_cut");
    waitForClipboard();
  },
  function () {
    is(clipboardText, testURL.substring(testURL.length - 10, testURL.length),
       "Clipboard has the correct value");
    is(gURLBar.value, testURL.substring(0, testURL.length - 10), "gURLBar.value has the correct value");

    // We're done, so just finish up
    cleanup();
  }
]

function test() {
  waitForExplicitFinish();
  testTab = gBrowser.addTab();
  gBrowser.selectedTab = testTab;

  // Kick off the testing
  runNextTest();
}
