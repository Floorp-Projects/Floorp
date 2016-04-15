/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var testURL = "http://example.org/browser/browser/base/content/test/urlbar/dummy_page.html";
var testActionURL = "moz-action:switchtab," + JSON.stringify({url: testURL});
testURL = gURLBar.trimValue(testURL);
var testTab;

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

var tests = [
  {
    expected: testURL,
    setup: function() {
      gURLBar.value = testActionURL;
      gURLBar.valueIsTyped = true;
      is(gURLBar.value, testActionURL, "gURLBar starts with the correct real value");
      is(gURLBar.textValue, testURL, "gURLBar starts with the correct display value");

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
      is(gURLBar.value, testActionURL, "gURLBar starts with the correct real value");
      is(gURLBar.textValue, testURL, "gURLBar starts with the correct display value");

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
