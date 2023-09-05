/* -*- Mode: JavaScript; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

async function test_user_input_handling_delay_aboutblank_helper(prefs) {
  await SpecialPowers.pushPrefEnv({
    set: prefs,
  });

  let newTabOpened = BrowserTestUtils.waitForNewTab(gBrowser, "about:blank");

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function () {
    // Open about:blank
    content.window.open();
  });

  const tab = await newTabOpened;

  let mouseDownPromise = BrowserTestUtils.waitForContentEvent(
    tab.linkedBrowser,
    "mousedown"
  ).then(function () {
    Assert.ok(true, "about:blank can handle user input events anytime");
  });

  // Now gBrowser.selectedBrowser is the newly opened about:blank
  await BrowserTestUtils.synthesizeMouseAtPoint(
    10,
    10,
    { type: "mousedown" },
    tab.linkedBrowser
  );

  await mouseDownPromise;
  BrowserTestUtils.removeTab(tab);
}

add_task(async function test_MinRAF_aboutblank() {
  const prefs = [
    ["dom.input_events.security.minNumTicks", 100],
    ["dom.input_events.security.minTimeElapsedInMS", 0],
    ["dom.input_events.security.isUserInputHandlingDelayTest", true],
  ];

  await test_user_input_handling_delay_aboutblank_helper(prefs);
});

add_task(async function test_MinElapsedTime_aboutblank() {
  const prefs = [
    ["dom.input_events.security.minNumTicks", 0],
    ["dom.input_events.security.minTimeElapsedInMS", 5000],
    ["dom.input_events.security.isUserInputHandlingDelayTest", true],
  ];

  await test_user_input_handling_delay_aboutblank_helper(prefs);
});
