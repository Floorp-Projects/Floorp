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

    waitForSuccess(waitForLocation1);
  }

  let waitForLocation1 = {
    name: "window.location.href result is displayed",
    validatorFn: function()
    {
      let node = hud.outputNode.getElementsByClassName("webconsole-msg-output")[0];
      return node && node.textContent.indexOf(TEST_URI1) > -1;
    },
    successFn: function()
    {
      let node = hud.outputNode.getElementsByClassName("webconsole-msg-input")[0];
      isnot(node.textContent.indexOf("window.location.href"), -1,
            "jsterm input is also displayed");

      is(hud.outputNode.textContent.indexOf("Permission denied"), -1,
         "no permission denied errors");

      gBrowser.selectedBrowser.addEventListener("load", onPageLoad2, true);
      content.location = TEST_URI2;
    },
    failureFn: finishTestWithError,
  };

  function onPageLoad2() {
    gBrowser.selectedBrowser.removeEventListener("load", onPageLoad2, true);

    hud.jsterm.clearOutput();
    hud.jsterm.execute("window.location.href");

    waitForSuccess(waitForLocation2);
  }

  let waitForLocation2 = {
    name: "window.location.href result is displayed after page navigation",
    validatorFn: function()
    {
      let node = hud.outputNode.getElementsByClassName("webconsole-msg-output")[0];
      return node && node.textContent.indexOf(TEST_URI2) > -1;
    },
    successFn: function()
    {
      let node = hud.outputNode.getElementsByClassName("webconsole-msg-input")[0];
      isnot(node.textContent.indexOf("window.location.href"), -1,
            "jsterm input is also displayed");
      is(hud.outputNode.textContent.indexOf("Permission denied"), -1,
         "no permission denied errors");

      gBrowser.goBack();
      waitForSuccess(waitForBack);
    },
    failureFn: finishTestWithError,
  };

  let waitForBack = {
    name: "go back",
    validatorFn: function()
    {
      return content.location.href == TEST_URI1;
    },
    successFn: function()
    {
      hud.jsterm.clearOutput();
      hud.jsterm.execute("window.location.href");

      waitForSuccess(waitForLocation3);
    },
    failureFn: finishTestWithError,
  };

  let waitForLocation3 = {
    name: "window.location.href result is displayed after goBack()",
    validatorFn: function()
    {
      let node = hud.outputNode.getElementsByClassName("webconsole-msg-output")[0];
      return node && node.textContent.indexOf(TEST_URI1) > -1;
    },
    successFn: function()
    {
      let node = hud.outputNode.getElementsByClassName("webconsole-msg-input")[0];
      isnot(node.textContent.indexOf("window.location.href"), -1,
            "jsterm input is also displayed");
      is(hud.outputNode.textContent.indexOf("Permission denied"), -1,
         "no permission denied errors");

      executeSoon(finishTest);
    },
    failureFn: finishTestWithError,
  };

  function finishTestWithError()
  {
    info("output content: " + hud.outputNode.textContent);
    finishTest();
  }
}
