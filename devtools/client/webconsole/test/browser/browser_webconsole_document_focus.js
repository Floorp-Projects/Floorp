/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that focus is restored to content page after closing the console. See Bug 588342.
const TEST_URI =
  "data:text/html;charset=utf-8,<!DOCTYPE html>Test content focus after closing console";

add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);

  info("Focus after console is opened");
  ok(isInputFocused(hud), "input node is focused after console is opened");

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function () {
    content.onFocus = new Promise(resolve => {
      content.addEventListener("focus", resolve, { once: true });
    });
  });

  info("Closing console");
  await closeConsole();

  const isFocused = await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    async function () {
      await content.onFocus;
      return Services.focus.focusedWindow == content;
    }
  );
  ok(isFocused, "content document has focus after closing the console");
});

add_task(async function testSeparateWindowToolbox() {
  const hud = await openNewTabAndConsole(TEST_URI, true, "window");

  info("Focus after console is opened");
  ok(isInputFocused(hud), "input node is focused after console is opened");

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function () {
    content.onFocus = new Promise(resolve => {
      content.addEventListener("focus", resolve, { once: true });
    });
  });

  info("Closing console");
  await closeConsole();

  const isFocused = await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    async function () {
      await content.onFocus;
      return Services.focus.focusedWindow == content;
    }
  );
  ok(isFocused, "content document has focus after closing the console");
});

add_task(async function testSeparateWindowToolboxInactiveTab() {
  await openNewTabAndConsole(TEST_URI, true, "window");

  info("Focus after console is opened");
  const firstTab = gBrowser.selectedTab;
  await addTab(`data:text/html,<!DOCTYPE html><meta charset=utf8>New tab XXX`);

  await SpecialPowers.spawn(firstTab.linkedBrowser, [], async () => {
    // For some reason, there is no blur event fired on the document
    await ContentTaskUtils.waitForCondition(
      () => !content.browsingContext.isActive && !content.document.hasFocus(),
      "Waiting for first tab to become inactive"
    );
    content.onFocus = new Promise(resolve => {
      content.addEventListener("focus", resolve, { once: true });
    });
  });

  info("Closing console");
  await closeConsole(firstTab);

  const onFirstTabFocus = SpecialPowers.spawn(
    firstTab.linkedBrowser,
    [],
    async function () {
      await content.onFocus;
      return "focused";
    }
  );
  const timeoutRes = "time's out";
  const onTimeout = wait(2000).then(() => timeoutRes);
  const res = await Promise.race([onFirstTabFocus, onTimeout]);
  is(
    res,
    timeoutRes,
    "original tab wasn't focused when closing the toolbox window"
  );
});
