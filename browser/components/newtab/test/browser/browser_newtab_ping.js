/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ASRouter } = ChromeUtils.import(
  "resource://activity-stream/lib/ASRouter.jsm"
);

let sendTriggerMessageSpy;

add_setup(function() {
  let sandbox = sinon.createSandbox();
  sendTriggerMessageSpy = sandbox.spy(ASRouter, "sendTriggerMessage");

  registerCleanupFunction(() => {
    sandbox.restore();
  });
});

add_task(async function test_newtab_tab_close_sends_ping() {
  Services.fog.testResetFOG();
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
  sendTriggerMessageSpy.resetHistory();

  let record = Glean.newtab.opened.testGetValue("newtab");
  Assert.equal(record.length, 1, "Should only be one open");
  const sessionId = record[0].extra.newtab_session_id;
  Assert.ok(!!sessionId, "newtab_session_id must be present");

  let pingSubmitted = false;
  GleanPings.newtab.testBeforeNextSubmit(reason => {
    pingSubmitted = true;
    Assert.equal(reason, "newtab_session_end");
    record = Glean.newtab.closed.testGetValue("newtab");
    Assert.equal(record.length, 1, "Should only have one close");
    Assert.equal(
      record[0].extra.newtab_session_id,
      sessionId,
      "Should've closed the session we opened"
    );
  });

  BrowserTestUtils.removeTab(tab);
  await BrowserTestUtils.waitForCondition(
    () => pingSubmitted,
    "We expect the ping to have submitted."
  );
});

add_task(async function test_newtab_tab_nav_sends_ping() {
  Services.fog.testResetFOG();
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
  sendTriggerMessageSpy.resetHistory();

  let record = Glean.newtab.opened.testGetValue("newtab");
  Assert.equal(record.length, 1, "Should only be one open");
  const sessionId = record[0].extra.newtab_session_id;
  Assert.ok(!!sessionId, "newtab_session_id must be present");

  let pingSubmitted = false;
  GleanPings.newtab.testBeforeNextSubmit(reason => {
    pingSubmitted = true;
    Assert.equal(reason, "newtab_session_end");
    record = Glean.newtab.closed.testGetValue("newtab");
    Assert.equal(record.length, 1, "Should only have one close");
    Assert.equal(
      record[0].extra.newtab_session_id,
      sessionId,
      "Should've closed the session we opened"
    );
  });

  BrowserTestUtils.loadURI(tab.linkedBrowser, "about:mozilla");
  await BrowserTestUtils.waitForCondition(
    () => pingSubmitted,
    "We expect the ping to have submitted."
  );

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_newtab_doesnt_send_pref() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.newtabpage.ping.enabled", false]],
  });
  Services.fog.testResetFOG();
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
  sendTriggerMessageSpy.resetHistory();

  GleanPings.newtab.testBeforeNextSubmit(() => {
    Assert.ok(false, "Must not submit ping!");
  });
  BrowserTestUtils.loadURI(tab.linkedBrowser, "about:mozilla");
  BrowserTestUtils.removeTab(tab);
  // There is no guarantee that the ping won't wait until it's out of the test
  // bounds to submit, but logging an assert failure late will still fail.
  // (I checked.)
});
