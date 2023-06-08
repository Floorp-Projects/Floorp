/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

const BODY_URL =
  "<body onpointerdown='this.requestPointerLock()' style='width: 100px; height: 100px;'></body>";

const TEST_URL = "data:text/html," + BODY_URL;

const FRAME_TEST_URL =
  'data:text/html,<body><iframe src="http://example.org/document-builder.sjs?html=' +
  encodeURI(BODY_URL) +
  '"></iframe></body>';

function checkWarningState(aWarningElement, aExpectedState, aMsg) {
  ["hidden", "ontop", "onscreen"].forEach(state => {
    is(
      aWarningElement.hasAttribute(state),
      state == aExpectedState,
      `${aMsg} - check ${state} attribute.`
    );
  });
}

async function waitForWarningState(aWarningElement, aExpectedState) {
  await BrowserTestUtils.waitForAttribute(aExpectedState, aWarningElement, "");
  checkWarningState(
    aWarningElement,
    aExpectedState,
    `Wait for ${aExpectedState} state`
  );
}

// Make sure the pointerlock warning is shown and exited with the escape key
add_task(async function show_pointerlock_warning_escape() {
  let urls = [TEST_URL, FRAME_TEST_URL];
  for (let url of urls) {
    info("Pointerlock warning test for " + url);

    let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);

    let warning = document.getElementById("pointerlock-warning");
    let warningShownPromise = waitForWarningState(warning, "onscreen");

    let expectedWarningText;

    let bc = tab.linkedBrowser.browsingContext;
    if (bc.children.length) {
      // use the subframe if it exists
      bc = bc.children[0];
      expectedWarningText = "example.org";
    } else {
      expectedWarningText = "This document";
    }
    expectedWarningText +=
      " has control of your pointer. Press Esc to take back control.";

    await BrowserTestUtils.synthesizeMouse("body", 4, 4, {}, bc);

    await warningShownPromise;

    ok(true, "Pointerlock warning shown");

    let warningHiddenPromise = waitForWarningState(warning, "hidden");

    await BrowserTestUtils.waitForCondition(
      () => warning.innerText == expectedWarningText,
      "Warning text"
    );

    EventUtils.synthesizeKey("KEY_Escape");
    await warningHiddenPromise;

    ok(true, "Pointerlock warning hidden");

    // Pointerlock should be released after escape is pressed.
    await SpecialPowers.spawn(tab.linkedBrowser, [], async function () {
      Assert.equal(content.document.pointerLockElement, null);
    });

    await BrowserTestUtils.removeTab(tab);
  }
});

/*
// XXX Bug 1580961 - this part of the test is disabled.
//
// Make sure the pointerlock warning is shown, but this time escape is not pressed until after the
// notification is closed via the timeout.
add_task(async function show_pointerlock_warning_timeout() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  let warning = document.getElementById("pointerlock-warning");
  let warningShownPromise = BrowserTestUtils.waitForAttribute(
    "onscreen",
    warning,
    "true"
  );
  let warningHiddenPromise = BrowserTestUtils.waitForAttribute(
    "hidden",
    warning,
    "true"
  );
  await BrowserTestUtils.synthesizeMouse("body", 4, 4, {}, tab.linkedBrowser);

  await warningShownPromise;
  ok(true, "Pointerlock warning shown");
  await warningHiddenPromise;

  // The warning closes after a few seconds, but this does not exit pointerlock mode.
  await SpecialPowers.spawn(tab.linkedBrowser, [], async function() {
    Assert.equal(content.document.pointerLockElement, content.document.body);
  });

  EventUtils.synthesizeKey("KEY_Escape");

  ok(true, "Pointerlock warning hidden");

  // Pointerlock should now be released.
  await SpecialPowers.spawn(tab.linkedBrowser, [], async function() {
    Assert.equal(content.document.pointerLockElement, null);
  });

  await BrowserTestUtils.removeTab(tab);
});
*/
