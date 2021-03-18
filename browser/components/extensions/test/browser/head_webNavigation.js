/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

/* exported BASE_URL, SOURCE_PAGE, OPENED_PAGE,
   runCreatedNavigationTargetTest, assertNoPendingCreatedNavigationTargetData */

const BASE_URL =
  "http://mochi.test:8888/browser/browser/components/extensions/test/browser";
const SOURCE_PAGE = `${BASE_URL}/webNav_createdTargetSource.html`;
const OPENED_PAGE = `${BASE_URL}/webNav_createdTarget.html`;

async function runCreatedNavigationTargetTest({
  extension,
  openNavTarget,
  expectedWebNavProps,
}) {
  await openNavTarget();

  const webNavMsg = await extension.awaitMessage("webNavOnCreated");
  const createdTabId = await extension.awaitMessage("tabsOnCreated");
  const completedNavMsg = await extension.awaitMessage("webNavOnCompleted");

  let { sourceTabId, sourceFrameId, url } = expectedWebNavProps;

  is(webNavMsg.tabId, createdTabId, "Got the expected tabId property");
  is(
    webNavMsg.sourceTabId,
    sourceTabId,
    "Got the expected sourceTabId property"
  );
  is(
    webNavMsg.sourceFrameId,
    sourceFrameId,
    "Got the expected sourceFrameId property"
  );
  is(webNavMsg.url, url, "Got the expected url property");

  is(
    completedNavMsg.tabId,
    createdTabId,
    "Got the expected webNavigation.onCompleted tabId property"
  );
  is(
    completedNavMsg.url,
    url,
    "Got the expected webNavigation.onCompleted url property"
  );
}

// Test that there are no pending createdNavigationTarget messages still tracked
// in WebNavigation.jsm (to be called before the extension is unloaded, because
// once the last extension which have subscribed a webNavigation event is unloaded
// all the pending created navigation target data is completely cleared).
function assertNoPendingCreatedNavigationTargetData() {
  const { Manager } = ChromeUtils.import(
    "resource://gre/modules/WebNavigation.jsm",
    null
  );
  Assert.equal(
    Manager.createdNavigationTargetByOuterWindowId.size,
    0,
    "There should be no pending createdNavigationTarget messages in WebNavigation"
  );
}
