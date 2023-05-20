/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ASRouter } = ChromeUtils.import(
  "resource://activity-stream/lib/ASRouter.jsm"
);

let sendTriggerMessageSpy;
let triggerMatch;

add_setup(function () {
  let sandbox = sinon.createSandbox();
  sendTriggerMessageSpy = sandbox.spy(ASRouter, "sendTriggerMessage");
  triggerMatch = sandbox.match({ id: "defaultBrowserCheck" });

  registerCleanupFunction(() => {
    sandbox.restore();
  });
});

async function testPageTrigger(url, waitForLoad, expectedTrigger) {
  sendTriggerMessageSpy.resetHistory();
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    url,
    waitForLoad
  );

  await BrowserTestUtils.waitForCondition(
    () => sendTriggerMessageSpy.calledWith(expectedTrigger),
    `After ${url} finishes loading`
  );
  Assert.ok(
    sendTriggerMessageSpy.calledWith(expectedTrigger),
    `Found the expected ${expectedTrigger.id} trigger`
  );

  BrowserTestUtils.removeTab(tab);
  sendTriggerMessageSpy.resetHistory();
}

add_task(function test_newtab_trigger() {
  return testPageTrigger("about:newtab", false, triggerMatch);
});

add_task(function test_abouthome_trigger() {
  return testPageTrigger("about:home", true, triggerMatch);
});
