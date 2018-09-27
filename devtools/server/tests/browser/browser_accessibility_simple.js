/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const PREF_ACCESSIBILITY_FORCE_DISABLED = "accessibility.force_disabled";

function checkAccessibilityState(accessibility, expected) {
  const { enabled, canBeDisabled, canBeEnabled } = accessibility;
  is(enabled, expected.enabled, "Enabled state is correct.");
  is(canBeDisabled, expected.canBeDisabled, "canBeDisabled state is correct.");
  is(canBeEnabled, expected.canBeEnabled, "canBeEnabled state is correct.");
}

// Simple checks for the AccessibilityActor and AccessibleWalkerActor

add_task(async function() {
  const { walker: domWalker, client, accessibility} = await initAccessibilityFrontForUrl(
    "data:text/html;charset=utf-8,<title>test</title><div></div>");

  ok(accessibility, "The AccessibilityFront was created");
  ok(accessibility.getWalker, "The getWalker method exists");

  let a11yWalker = await accessibility.getWalker();
  ok(a11yWalker, "The AccessibleWalkerFront was returned");

  checkAccessibilityState(accessibility,
    { enabled: false, canBeDisabled: true, canBeEnabled: true });

  info("Force disable accessibility service: updates canBeEnabled flag");
  let onEvent = accessibility.once("can-be-enabled-change");
  Services.prefs.setIntPref(PREF_ACCESSIBILITY_FORCE_DISABLED, 1);
  await onEvent;
  checkAccessibilityState(accessibility,
    { enabled: false, canBeDisabled: true, canBeEnabled: false });

  info("Clear force disable accessibility service: updates canBeEnabled flag");
  onEvent = accessibility.once("can-be-enabled-change");
  Services.prefs.clearUserPref(PREF_ACCESSIBILITY_FORCE_DISABLED);
  await onEvent;
  checkAccessibilityState(accessibility,
    { enabled: false, canBeDisabled: true, canBeEnabled: true });

  info("Initialize accessibility service");
  const initEvent = accessibility.once("init");
  await accessibility.enable();
  await waitForA11yInit();
  await initEvent;
  checkAccessibilityState(accessibility,
    { enabled: true, canBeDisabled: true, canBeEnabled: true });

  a11yWalker = await accessibility.getWalker();
  const rootNode = await domWalker.getRootNode();
  const a11yDoc = await a11yWalker.getAccessibleFor(rootNode);
  ok(a11yDoc, "Accessible document actor is created");

  info("Shutdown accessibility service");
  const shutdownEvent = accessibility.once("shutdown");
  await accessibility.disable();
  await waitForA11yShutdown();
  await shutdownEvent;
  checkAccessibilityState(accessibility,
    { enabled: false, canBeDisabled: true, canBeEnabled: true });

  await client.close();
  gBrowser.removeCurrentTab();
});
