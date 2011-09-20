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

let testURL = "http://example.org/browser/browser/base/content/test/dummy_page.html";
let testActionURL = "moz-action:switchtab," + testURL;
testURL = gURLBar.trimValue(testURL);
let testTab;

function runNextTest() {
  if (tests.length) {
    let t = tests.shift();
    waitForClipboard(t.expected, t.setup, function() {
      t.success();
      runNextTest();
    }, cleanup);
  }
  else {
    cleanup();
  }
}

function cleanup() {
  gBrowser.removeTab(testTab);
  finish();
}

let tests = [
  {
    expected: testURL,
    setup: function() {
      gURLBar.value = testActionURL;
      gURLBar.valueIsTyped = true;
      is(gURLBar.value, testActionURL, "gURLBar.value starts with correct value");

      // Focus the urlbar so we can select it all & copy
      gURLBar.focus();
      gURLBar.select();
      goDoCommand("cmd_copy");
    },
    success: function() {
      is(gURLBar.value, testActionURL, "gURLBar.value didn't change when copying");
    }
  },
  {
    expected: testURL.substring(0, 10),
    setup: function() {
      // Set selectionStart/End manually and make sure it matches the substring
      gURLBar.selectionStart = 0;
      gURLBar.selectionEnd = 10;
      goDoCommand("cmd_copy");
    },
    success: function() {
      is(gURLBar.value, testActionURL, "gURLBar.value didn't change when copying");
    }
  },
  {
    expected: testURL,
    setup: function() {
      // Setup for cut test...
      // Select all
      gURLBar.select();
      goDoCommand("cmd_cut");
    },
    success: function() {
      is(gURLBar.value, "", "gURLBar.value is now empty");
    }
  },
  {
    expected: testURL.substring(testURL.length - 10, testURL.length),
    setup: function() {
      // Reset urlbar value
      gURLBar.value = testActionURL;
      gURLBar.valueIsTyped = true;
      // Sanity check that we have the right value
      is(gURLBar.value, testActionURL, "gURLBar.value starts with correct value");

      // Now just select part of the value & cut that.
      gURLBar.selectionStart = testURL.length - 10;
      gURLBar.selectionEnd = testURL.length;
      goDoCommand("cmd_cut");
    },
    success: function() {
      is(gURLBar.value, testURL.substring(0, testURL.length - 10), "gURLBar.value has the correct value");
    }
  }
];

function test() {
  waitForExplicitFinish();
  testTab = gBrowser.addTab();
  gBrowser.selectedTab = testTab;

  // Kick off the testing
  runNextTest();
}
