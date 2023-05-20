/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const PREF_ACCESSIBILITY_FORCE_DISABLED = "accessibility.force_disabled";

function checkAccessibilityState(accessibility, parentAccessibility, expected) {
  const { enabled } = accessibility;
  const { canBeDisabled, canBeEnabled } = parentAccessibility;
  is(enabled, expected.enabled, "Enabled state is correct.");
  is(canBeDisabled, expected.canBeDisabled, "canBeDisabled state is correct.");
  is(canBeEnabled, expected.canBeEnabled, "canBeEnabled state is correct.");
}

// Simple checks for the AccessibilityActor and AccessibleWalkerActor

add_task(async function () {
  const {
    walker: domWalker,
    target,
    accessibility,
    parentAccessibility,
    a11yWalker,
  } = await initAccessibilityFrontsForUrl(
    "data:text/html;charset=utf-8,<title>test</title><div></div>",
    { enableByDefault: false }
  );

  ok(accessibility, "The AccessibilityFront was created");
  ok(accessibility.getWalker, "The getWalker method exists");
  ok(accessibility.getSimulator, "The getSimulator method exists");

  ok(accessibility.accessibleWalkerFront, "Accessible walker was initialized");

  is(
    a11yWalker,
    accessibility.accessibleWalkerFront,
    "The AccessibleWalkerFront was returned"
  );

  const a11ySimulator = accessibility.simulatorFront;
  ok(accessibility.simulatorFront, "Accessible simulator was initialized");
  is(
    a11ySimulator,
    accessibility.simulatorFront,
    "The SimulatorFront was returned"
  );

  checkAccessibilityState(accessibility, parentAccessibility, {
    enabled: false,
    canBeDisabled: true,
    canBeEnabled: true,
  });

  info("Force disable accessibility service: updates canBeEnabled flag");
  let onEvent = parentAccessibility.once("can-be-enabled-change");
  Services.prefs.setIntPref(PREF_ACCESSIBILITY_FORCE_DISABLED, 1);
  await onEvent;
  checkAccessibilityState(accessibility, parentAccessibility, {
    enabled: false,
    canBeDisabled: true,
    canBeEnabled: false,
  });

  info("Clear force disable accessibility service: updates canBeEnabled flag");
  onEvent = parentAccessibility.once("can-be-enabled-change");
  Services.prefs.clearUserPref(PREF_ACCESSIBILITY_FORCE_DISABLED);
  await onEvent;
  checkAccessibilityState(accessibility, parentAccessibility, {
    enabled: false,
    canBeDisabled: true,
    canBeEnabled: true,
  });

  info("Initialize accessibility service");
  const initEvent = accessibility.once("init");
  await parentAccessibility.enable();
  await waitForA11yInit();
  await initEvent;
  checkAccessibilityState(accessibility, parentAccessibility, {
    enabled: true,
    canBeDisabled: true,
    canBeEnabled: true,
  });

  const rootNode = await domWalker.getRootNode();
  const a11yDoc = await accessibility.accessibleWalkerFront.getAccessibleFor(
    rootNode
  );
  ok(a11yDoc, "Accessible document actor is created");

  info("Shutdown accessibility service");
  const shutdownEvent = accessibility.once("shutdown");
  await waitForA11yShutdown(parentAccessibility);
  await shutdownEvent;
  checkAccessibilityState(accessibility, parentAccessibility, {
    enabled: false,
    canBeDisabled: true,
    canBeEnabled: true,
  });

  await target.destroy();
  gBrowser.removeCurrentTab();
});
