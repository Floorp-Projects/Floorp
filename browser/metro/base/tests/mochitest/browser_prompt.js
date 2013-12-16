/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function test() {
  waitForExplicitFinish();
  runTests();
}

gTests.push({
  desc: "Test getPrompt with null domWindow (bug 949333)",
  run: function () {
    let factory = Cc["@mozilla.org/prompter;1"].getService(Ci.nsIPromptFactory);
    let prompter = factory.getPrompt(null, Ci.nsIPrompt);

    let browser = getBrowser();
    let dialogOpened = false;

    Task.spawn(function() {
      yield waitForEvent(browser, "DOMWillOpenModalDialog");
      dialogOpened = true;

      yield waitForMs(0);
      let stack = browser.parentNode;
      let dialogs = stack.getElementsByTagNameNS(XUL_NS, "tabmodalprompt");
      is(dialogs.length, 1, "one tab-modal dialog showing");

      let dialog = dialogs[0];
      let okButton = dialog.ui.button0;
      okButton.click();
    });

    prompter.alert("Test", "test");

    ok(dialogOpened, "Dialog was opened");
  }
});
