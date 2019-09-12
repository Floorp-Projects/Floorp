/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

const TEST_URL = "data:text/html,<body onpointerdown='this.requestPointerLock()' style='width: 100px; height: 100px;'></body>";

// Make sure the pointerlock warning is shown and exited with the escape key
add_task(async function show_pointerlock_warning_escape() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  let warning = document.getElementById("pointerlock-warning");
  let warningShownPromise = BrowserTestUtils.waitForAttribute("onscreen", warning, "true");
  await BrowserTestUtils.synthesizeMouse("body", 4, 4, {}, tab.linkedBrowser);

  await warningShownPromise;

  ok(true, "Pointerlock warning shown");

  let warningHiddenPromise = BrowserTestUtils.waitForAttribute("hidden", warning, "true");
  EventUtils.synthesizeKey("KEY_Escape");
  await warningHiddenPromise;

  ok(true, "Pointerlock warning hidden");

  // Pointerlock should be released after escape is pressed.
  await ContentTask.spawn(tab.linkedBrowser, null, async function() {
    Assert.equal(content.document.pointerLockElement, null);
  });

  await BrowserTestUtils.removeTab(tab);
});

// Make sure the pointerlock warning is shown, but this time escape is not pressed until after the
// notification is closed via the timeout.
add_task(async function show_pointerlock_warning_timeout() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  let warning = document.getElementById("pointerlock-warning");
  let warningShownPromise = BrowserTestUtils.waitForAttribute("onscreen", warning, "true");
  let warningHiddenPromise = BrowserTestUtils.waitForAttribute("hidden", warning, "true");
  await BrowserTestUtils.synthesizeMouse("body", 4, 4, {}, tab.linkedBrowser);

  await warningShownPromise;
  ok(true, "Pointerlock warning shown");
  await warningHiddenPromise;

  // The warning closes after a few seconds, but this does not exit pointerlock mode.
  await ContentTask.spawn(tab.linkedBrowser, null, async function() {
    Assert.equal(content.document.pointerLockElement, content.document.body);
  });

  EventUtils.synthesizeKey("KEY_Escape");

  ok(true, "Pointerlock warning hidden");

  // Pointerlock should now be released.
  await ContentTask.spawn(tab.linkedBrowser, null, async function() {
    Assert.equal(content.document.pointerLockElement, null);
  });

  await BrowserTestUtils.removeTab(tab);
});

