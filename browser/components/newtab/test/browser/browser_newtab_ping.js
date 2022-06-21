/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { AboutNewTab } = ChromeUtils.import(
  "resource:///modules/AboutNewTab.jsm"
);
const { ASRouter } = ChromeUtils.import(
  "resource://activity-stream/lib/ASRouter.jsm"
);

const { ExperimentFakes } = ChromeUtils.import(
  "resource://testing-common/NimbusTestUtils.jsm"
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
  await SpecialPowers.pushPrefEnv({
    set: [["browser.newtabpage.activity-stream.telemetry", true]],
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

  await BrowserTestUtils.waitForCondition(
    () => !!Glean.newtab.opened.testGetValue("newtab"),
    "We expect the newtab open to be recorded"
  );
  let record = Glean.newtab.opened.testGetValue("newtab");
  Assert.equal(record.length, 1, "Should only be one open");
  const sessionId = record[0].extra.newtab_visit_id;
  Assert.ok(!!sessionId, "newtab_visit_id must be present");

  let pingSubmitted = false;
  GleanPings.newtab.testBeforeNextSubmit(reason => {
    pingSubmitted = true;
    Assert.equal(reason, "newtab_session_end");
    record = Glean.newtab.closed.testGetValue("newtab");
    Assert.equal(record.length, 1, "Should only have one close");
    Assert.equal(
      record[0].extra.newtab_visit_id,
      sessionId,
      "Should've closed the session we opened"
    );
  });

  BrowserTestUtils.removeTab(tab);
  await BrowserTestUtils.waitForCondition(
    () => pingSubmitted,
    "We expect the ping to have submitted."
  );
  await SpecialPowers.popPrefEnv();
});

add_task(async function test_newtab_tab_nav_sends_ping() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.newtabpage.activity-stream.telemetry", true]],
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

  await BrowserTestUtils.waitForCondition(
    () => !!Glean.newtab.opened.testGetValue("newtab"),
    "We expect the newtab open to be recorded"
  );
  let record = Glean.newtab.opened.testGetValue("newtab");
  Assert.equal(record.length, 1, "Should only be one open");
  const sessionId = record[0].extra.newtab_visit_id;
  Assert.ok(!!sessionId, "newtab_visit_id must be present");

  let pingSubmitted = false;
  GleanPings.newtab.testBeforeNextSubmit(reason => {
    pingSubmitted = true;
    Assert.equal(reason, "newtab_session_end");
    record = Glean.newtab.closed.testGetValue("newtab");
    Assert.equal(record.length, 1, "Should only have one close");
    Assert.equal(
      record[0].extra.newtab_visit_id,
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
  await SpecialPowers.popPrefEnv();
});

add_task(async function test_newtab_doesnt_send_nimbus() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.newtabpage.activity-stream.telemetry", true]],
  });

  let doEnrollmentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "glean",
    value: { newtabPingEnabled: false },
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

  await BrowserTestUtils.waitForCondition(
    () => !!Glean.newtab.opened.testGetValue("newtab"),
    "We expect the newtab open to be recorded"
  );
  let record = Glean.newtab.opened.testGetValue("newtab");
  Assert.equal(record.length, 1, "Should only be one open");
  const sessionId = record[0].extra.newtab_visit_id;
  Assert.ok(!!sessionId, "newtab_visit_id must be present");

  GleanPings.newtab.testBeforeNextSubmit(() => {
    Assert.ok(false, "Must not submit ping!");
  });
  BrowserTestUtils.loadURI(tab.linkedBrowser, "about:mozilla");
  BrowserTestUtils.removeTab(tab);
  await BrowserTestUtils.waitForCondition(() => {
    let { sessions } = AboutNewTab.activityStream.store.feeds.get(
      "feeds.telemetry"
    );
    return !Array.from(sessions.entries()).filter(
      ([k, v]) => v.session_id === sessionId
    ).length;
  }, "Waiting for sessions to clean up.");
  // Session ended without a ping being sent. Success!
  await doEnrollmentCleanup();
  await SpecialPowers.popPrefEnv();
});
