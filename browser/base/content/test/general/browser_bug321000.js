/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const kTestString = "  hello hello  \n  world\nworld  ";

var gTests = [

  { desc: "Urlbar strips newlines and surrounding whitespace",
    element: gURLBar,
    expected: kTestString.replace(/\s*\n\s*/g, "")
  },

  { desc: "Searchbar replaces newlines with spaces",
    element: document.getElementById("searchbar"),
    expected: kTestString.replace(/\n/g, " ")
  },

];

// Test for bug 23485 and bug 321000.
// Urlbar should strip newlines,
// search bar should replace newlines with spaces.
function test() {
  waitForExplicitFinish();

  let cbHelper = Cc["@mozilla.org/widget/clipboardhelper;1"].
                 getService(Ci.nsIClipboardHelper);

  // Put a multi-line string in the clipboard.
  // Setting the clipboard value is an async OS operation, so we need to poll
  // the clipboard for valid data before going on.
  waitForClipboard(kTestString, function() { cbHelper.copyString(kTestString); },
                   next_test, finish);
}

function next_test() {
  if (gTests.length)
    test_paste(gTests.shift());
  else
    finish();
}

function test_paste(aCurrentTest) {
  var element = aCurrentTest.element;

  // Register input listener.
  var inputListener = {
    test: aCurrentTest,
    handleEvent(event) {
      element.removeEventListener(event.type, this);

      is(element.value, this.test.expected, this.test.desc);

      // Clear the field and go to next test.
      element.value = "";
      setTimeout(next_test, 0);
    }
  };
  element.addEventListener("input", inputListener);

  // Focus the window.
  window.focus();
  gBrowser.selectedBrowser.focus();

  // Focus the element and wait for focus event.
  info("About to focus " + element.id);
  element.addEventListener("focus", function() {
    executeSoon(function() {
      // Pasting is async because the Accel+V codepath ends up going through
      // nsDocumentViewer::FireClipboardEvent.
      info("Pasting into " + element.id);
      EventUtils.synthesizeKey("v", { accelKey: true });
    });
  }, {once: true});
  element.focus();
}
