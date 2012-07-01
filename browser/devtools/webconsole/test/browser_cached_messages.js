/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-webconsole-error-observer.html";

function test()
{
  waitForExplicitFinish();

  expectUncaughtException();

  addTab(TEST_URI);
  gBrowser.selectedBrowser.addEventListener("load", function onLoad() {
    gBrowser.selectedBrowser.removeEventListener("load", onLoad, true);
    testOpenUI(true);
  }, true);
}

function testOpenUI(aTestReopen)
{
  // test to see if the messages are
  // displayed when the console UI is opened

  let messages = {
    "log Bazzle" : false,
    "error Bazzle" : false,
    "bazBug611032" : false,
    "cssColorBug611032" : false,
  };

  openConsole(null, function(hud) {
    waitForSuccess({
      name: "cached messages displayed",
      validatorFn: function()
      {
        let foundAll = true;
        for (let msg in messages) {
          let found = messages[msg];
          if (!found) {
            found = hud.outputNode.textContent.indexOf(msg) > -1;
            if (found) {
              info("found message '" + msg + "'");
              messages[msg] = found;
            }
          }
          foundAll = foundAll && found;
        }
        return foundAll;
      },
      successFn: function()
      {
        // Make sure the CSS warning is given the correct category - bug 768019.
        let cssNode = hud.outputNode.querySelector(".webconsole-msg-cssparser");
        ok(cssNode, "CSS warning message element");
        isnot(cssNode.textContent.indexOf("cssColorBug611032"), -1,
              "CSS warning message element content is correct");

        closeConsole(gBrowser.selectedTab, function() {
          aTestReopen && info("will reopen the Web Console");
          executeSoon(aTestReopen ? testOpenUI : finishTest);
        });
      },
      failureFn: function()
      {
        for (let msg in messages) {
          if (!messages[msg]) {
            ok(false, "failed to find '" + msg + "'");
          }
        }
        finishTest();
      },
    });
  });
}
