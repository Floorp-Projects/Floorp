/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests if the JSTerm sandbox is updated when the user navigates from one
// domain to another, in order to avoid permission denied errors with a sandbox
// created for a different origin.

function test()
{
  const TEST_URI1 = "http://example.com/browser/browser/devtools/webconsole/test/test-console.html";
  const TEST_URI2 = "http://example.org/browser/browser/devtools/webconsole/test/test-console.html";

  let hud;
  let msgForLocation1;

  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab(TEST_URI1);
  gBrowser.selectedBrowser.addEventListener("load", function onLoad() {
    gBrowser.selectedBrowser.removeEventListener("load", onLoad, true);
    openConsole(gBrowser.selectedTab, pageLoad1);
  }, true);

  function pageLoad1(aHud)
  {
    hud = aHud;

    hud.jsterm.clearOutput();
    hud.jsterm.execute("window.location.href");

    info("wait for window.location.href");

    msgForLocation1 = {
      webconsole: hud,
      messages: [
        {
          name: "window.location.href jsterm input",
          text: "window.location.href",
          category: CATEGORY_INPUT,
        },
        {
          name: "window.location.href result is displayed",
          text: TEST_URI1,
          category: CATEGORY_OUTPUT,
        },
      ]
    };

    waitForMessages(msgForLocation1).then(() => {
      gBrowser.selectedBrowser.addEventListener("load", onPageLoad2, true);
      content.location = TEST_URI2;
    });
  }

  function onPageLoad2() {
    gBrowser.selectedBrowser.removeEventListener("load", onPageLoad2, true);

    is(hud.outputNode.textContent.indexOf("Permission denied"), -1,
       "no permission denied errors");

    hud.jsterm.clearOutput();
    hud.jsterm.execute("window.location.href");

    info("wait for window.location.href after page navigation");

    waitForMessages({
      webconsole: hud,
      messages: [
        {
          name: "window.location.href jsterm input",
          text: "window.location.href",
          category: CATEGORY_INPUT,
        },
        {
          name: "window.location.href result is displayed",
          text: TEST_URI2,
          category: CATEGORY_OUTPUT,
        },
      ]
    }).then(() => {
      is(hud.outputNode.textContent.indexOf("Permission denied"), -1,
         "no permission denied errors");

      gBrowser.goBack();
      waitForSuccess(waitForBack);
    });
  }

  let waitForBack = {
    name: "go back",
    validatorFn: function()
    {
      return content.location.href == TEST_URI1;
    },
    successFn: function()
    {
      hud.jsterm.clearOutput();
      executeSoon(() => {
        hud.jsterm.execute("window.location.href");
      });

      info("wait for window.location.href after goBack()");
      waitForMessages(msgForLocation1).then(() => executeSoon(() => {
        is(hud.outputNode.textContent.indexOf("Permission denied"), -1,
           "no permission denied errors");
        finishTest();
      }));
    },
    failureFn: finishTest,
  };
}
