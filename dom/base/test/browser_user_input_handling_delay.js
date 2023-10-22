/* -*- Mode: JavaScript; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

async function test_user_input_handling_delay_helper(prefs) {
  await SpecialPowers.pushPrefEnv({
    set: prefs,
  });

  const tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    `data:text/html,<body></body>`
  );

  let canHandleInput = false;
  let mouseDownPromise = BrowserTestUtils.waitForContentEvent(
    tab.linkedBrowser,
    "mousedown"
  ).then(function () {
    Assert.ok(
      canHandleInput,
      "This promise should be resolved after the 5 seconds mark has passed"
    );
  });

  for (let i = 0; i < 10; ++i) {
    await BrowserTestUtils.synthesizeMouseAtPoint(
      10,
      10,
      { type: "mousedown" },
      tab.linkedBrowser
    );
  }
  // Wait for roughly 5 seconds to give chances for the
  // above mousedown event to be handled.
  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    for (let i = 0; i < 330; ++i) {
      await new Promise(r => {
        content.requestAnimationFrame(r);
      });
    }
  });

  // If any user input events were handled in the above 5 seconds
  // the mouseDownPromise would be resolved with canHandleInput = false,
  // so that the test would fail.
  canHandleInput = true;

  // Ensure the events can be handled eventually
  await BrowserTestUtils.synthesizeMouseAtPoint(
    10,
    10,
    { type: "mousedown" },
    tab.linkedBrowser
  );

  await mouseDownPromise;

  BrowserTestUtils.removeTab(tab);
}

add_task(async function test_MinRAF() {
  const prefs = [
    ["dom.input_events.security.minNumTicks", 100],
    ["dom.input_events.security.minTimeElapsedInMS", 0],
    ["dom.input_events.security.isUserInputHandlingDelayTest", true],
  ];

  await test_user_input_handling_delay_helper(prefs);
});

add_task(async function test_MinElapsedTime() {
  const prefs = [
    ["dom.input_events.security.minNumTicks", 0],
    ["dom.input_events.security.minTimeElapsedInMS", 5000],
    ["dom.input_events.security.isUserInputHandlingDelayTest", true],
  ];

  await test_user_input_handling_delay_helper(prefs);
});
