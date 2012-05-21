/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests that console groups behave properly.

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-console.html";

function test() {
  addTab(TEST_URI);
  browser.addEventListener("DOMContentLoaded", testGroups, false);
}

function testGroups() {
  browser.removeEventListener("DOMContentLoaded", testGroups, false);

  openConsole();

  let HUD = HUDService.getHudByWindow(content);
  let jsterm = HUD.jsterm;
  let outputNode = jsterm.outputNode;

  // We test for one group by testing for zero "new" groups. The
  // "webconsole-new-group" class creates a divider. Thus one group is
  // indicated by zero new groups, two groups are indicated by one new group,
  // and so on.

  let timestamp0 = Date.now();
  jsterm.execute("0");
  is(outputNode.querySelectorAll(".webconsole-new-group").length, 0,
     "no group dividers exist after the first console message");

  jsterm.execute("1");
  let timestamp1 = Date.now();
  if (timestamp1 - timestamp0 < 5000) {
    is(outputNode.querySelectorAll(".webconsole-new-group").length, 0,
       "no group dividers exist after the second console message");
  }

  for (let i = 0; i < outputNode.itemCount; i++) {
    outputNode.getItemAtIndex(i).timestamp = 0;   // a "far past" value
  }

  jsterm.execute("2");
  is(outputNode.querySelectorAll(".webconsole-new-group").length, 1,
     "one group divider exists after the third console message");

  jsterm.clearOutput();
  jsterm.history.splice(0, jsterm.history.length);   // workaround for bug 592552

  finishTest();
}

