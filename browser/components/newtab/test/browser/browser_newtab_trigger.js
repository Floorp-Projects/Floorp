/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ASRouter } = ChromeUtils.import(
  "resource://activity-stream/lib/ASRouter.jsm"
);

let sendTriggerMessageSpy;

add_task(function setup() {
  let sandbox = sinon.createSandbox();
  sendTriggerMessageSpy = sandbox.spy(ASRouter, "sendTriggerMessage");

  registerCleanupFunction(() => {
    sandbox.restore();
  });
});

add_task(async function test_newtab_trigger() {
  sendTriggerMessageSpy.resetHistory();
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:newtab",
    false // waitForLoad; about:newtab is cached so this would never resolve
  );

  await BrowserTestUtils.waitForCondition(
    () => sendTriggerMessageSpy.called,
    "After about:newtab finishes loading"
  );

  Assert.equal(
    sendTriggerMessageSpy.firstCall.args[0].id,
    "defaultBrowserCheck",
    "Found the expected trigger"
  );
  BrowserTestUtils.removeTab(tab);
  sendTriggerMessageSpy.resetHistory();
});

add_task(async function test_abouthome_trigger() {
  sendTriggerMessageSpy.resetHistory();
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:home");

  await BrowserTestUtils.waitForCondition(
    () => sendTriggerMessageSpy.called,
    "After about:newtab finishes loading"
  );

  Assert.equal(
    sendTriggerMessageSpy.firstCall.args[0].id,
    "defaultBrowserCheck",
    "Found the expected trigger"
  );
  BrowserTestUtils.removeTab(tab);
  sendTriggerMessageSpy.resetHistory();
});
