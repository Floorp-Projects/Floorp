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

  await BrowserTestUtils.reloadTab(tab);

  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(r => setTimeout(r, 5000));

  const userInputHappend = SpecialPowers.spawn(
    tab.linkedBrowser,
    [],
    async function () {
      await ContentTaskUtils.waitForEvent(content, "keydown");
    }
  ).then(function () {
    Assert.ok(
      true,
      "User input event should be able to work after 5 seconds of an reload"
    );
  });

  // In the buggy build, the following tab key doesn't work
  await BrowserTestUtils.synthesizeKey("KEY_Tab", {}, tab.linkedBrowser);
  await BrowserTestUtils.synthesizeKey("KEY_Tab", {}, tab.linkedBrowser);
  await BrowserTestUtils.synthesizeKey("KEY_Tab", {}, tab.linkedBrowser);
  await BrowserTestUtils.synthesizeKey("KEY_Tab", {}, tab.linkedBrowser);

  await userInputHappend;

  BrowserTestUtils.removeTab(tab);
}

add_task(async function test_MinTick() {
  const prefs = [
    ["dom.input_events.security.minNumTicks", 10],
    ["dom.input_events.security.minTimeElapsedInMS", 0],
    ["dom.input_events.security.isUserInputHandlingDelayTest", true],
  ];

  await test_user_input_handling_delay_helper(prefs);
});
