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
    `
    data:text/html,
    <body>
      <iframe width="1" height="1" style="position:absolute; top:-9999px; left: -9999px; border-style: none" src="https://example.org/dom/base/test/empty.html"></iframe>
      <input />
    </body>`
  );

  await BrowserTestUtils.reloadTab(tab);

  let iframeFocused = SpecialPowers.spawn(
    tab.linkedBrowser,
    [],
    async function () {
      let iframe = content.document.querySelector("iframe");
      await ContentTaskUtils.waitForCondition(function () {
        return content.document.activeElement == iframe;
      });
    }
  );

  // Now the focus moves to the cross origin iframe
  await SpecialPowers.spawn(tab.linkedBrowser, [], async function () {
    content.document.querySelector("iframe").focus();
  });

  await iframeFocused;

  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(r => setTimeout(r, 1000));

  const inputGetFocused = SpecialPowers.spawn(
    tab.linkedBrowser,
    [],
    async function () {
      await ContentTaskUtils.waitForEvent(
        content.document.querySelector("input"),
        "focus"
      );
    }
  ).then(function () {
    Assert.ok(
      true,
      "Invisible OOP iframe shouldn't prevent user input event handling"
    );
  });

  let iframeBC = tab.linkedBrowser.browsingContext.children[0];
  // Next tab key should move the focus from the iframe to link
  await BrowserTestUtils.synthesizeKey("KEY_Tab", {}, iframeBC);

  await inputGetFocused;

  BrowserTestUtils.removeTab(tab);
}

add_task(async function test_InvisibleIframe() {
  const prefs = [
    ["dom.input_events.security.minNumTicks", 3],
    ["dom.input_events.security.minTimeElapsedInMS", 0],
    ["dom.input_events.security.isUserInputHandlingDelayTest", true],
  ];

  await test_user_input_handling_delay_helper(prefs);
});
