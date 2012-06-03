/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests that console groups behave properly.

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-console.html";

function test() {
  addTab(TEST_URI);
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    openConsole(null, testGroups);
  }, true);
}

function testGroups(HUD) {
  let jsterm = HUD.jsterm;
  let outputNode = HUD.outputNode;
  jsterm.clearOutput();

  // We test for one group by testing for zero "new" groups. The
  // "webconsole-new-group" class creates a divider. Thus one group is
  // indicated by zero new groups, two groups are indicated by one new group,
  // and so on.

  let waitForSecondMessage = {
    name: "second console message",
    validatorFn: function()
    {
      return outputNode.querySelectorAll(".webconsole-msg-output").length == 2;
    },
    successFn: function()
    {
      let timestamp1 = Date.now();
      if (timestamp1 - timestamp0 < 5000) {
        is(outputNode.querySelectorAll(".webconsole-new-group").length, 0,
           "no group dividers exist after the second console message");
      }

      for (let i = 0; i < outputNode.itemCount; i++) {
        outputNode.getItemAtIndex(i).timestamp = 0;   // a "far past" value
      }

      jsterm.execute("2");
      waitForSuccess(waitForThirdMessage);
    },
    failureFn: finishTest,
  };

  let waitForThirdMessage = {
    name: "one group divider exists after the third console message",
    validatorFn: function()
    {
      return outputNode.querySelectorAll(".webconsole-new-group").length == 1;
    },
    successFn: finishTest,
    failureFn: finishTest,
  };

  let timestamp0 = Date.now();
  jsterm.execute("0");

  waitForSuccess({
    name: "no group dividers exist after the first console message",
    validatorFn: function()
    {
      return outputNode.querySelectorAll(".webconsole-new-group").length == 0;
    },
    successFn: function()
    {
      jsterm.execute("1");
      waitForSuccess(waitForSecondMessage);
    },
    failureFn: finishTest,
  });
}
