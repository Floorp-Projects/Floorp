/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Check that basic keyboard shortcuts work in the web console.

"use strict";

const TEST_URI = `data:text/html;charset=utf-8,<!DOCTYPE html><p>Test keyboard accessibility</p>
  <script>
    for (let i = 1; i <= 100; i++) {
      console.log("console message " + i);
    }
  </script>
  `;

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);
  info("Web Console opened");
  const outputScroller = hud.ui.outputScroller;
  await waitFor(
    () => findConsoleAPIMessage(hud, "console message 100"),
    "waiting for all the messages to be displayed",
    100,
    1000
  );

  let currentPosition = outputScroller.scrollTop;
  const bottom = currentPosition;
  hud.jsterm.focus();
  // Page up.
  EventUtils.synthesizeKey("KEY_PageUp");
  isnot(
    outputScroller.scrollTop,
    currentPosition,
    "scroll position changed after page up"
  );
  // Page down.
  currentPosition = outputScroller.scrollTop;
  EventUtils.synthesizeKey("KEY_PageDown");
  ok(
    outputScroller.scrollTop > currentPosition,
    "scroll position now at bottom"
  );

  // Home
  EventUtils.synthesizeKey("KEY_Home");
  is(outputScroller.scrollTop, 0, "scroll position now at top");

  // End
  EventUtils.synthesizeKey("KEY_End");
  const scrollTop = outputScroller.scrollTop;
  ok(
    scrollTop > 0 && Math.abs(scrollTop - bottom) <= 5,
    "scroll position now at bottom"
  );

  // Clear output
  info("try ctrl-l to clear output");
  let clearShortcut;
  if (Services.appinfo.OS === "Darwin") {
    clearShortcut = WCUL10n.getStr("webconsole.clear.keyOSX");
  } else {
    clearShortcut = WCUL10n.getStr("webconsole.clear.key");
  }
  synthesizeKeyShortcut(clearShortcut);
  await waitFor(() => !findAllMessages(hud).length);
  ok(isInputFocused(hud), "console was cleared and input is focused");

  if (Services.appinfo.OS === "Darwin") {
    info("Log a new message from the content page");
    const onMessage = waitForMessageByType(
      hud,
      "another simple text message",
      ".console-api"
    );
    SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function() {
      content.console.log("another simple text message");
    });
    await onMessage;

    info("Send Cmd-K to clear console");
    synthesizeKeyShortcut(WCUL10n.getStr("webconsole.clear.alternativeKeyOSX"));

    await waitFor(() => !findAllMessages(hud).length);
    ok(
      isInputFocused(hud),
      "console was cleared as expected with alternative shortcut"
    );
  }

  // Focus filter
  info("try ctrl-f to focus filter");
  synthesizeKeyShortcut(WCUL10n.getStr("webconsole.find.key"));
  ok(!isInputFocused(hud), "input is not focused");
  ok(hasFocus(getFilterInput(hud)), "filter input is focused");

  info("try ctrl-f when filter is already focused");
  synthesizeKeyShortcut(WCUL10n.getStr("webconsole.find.key"));
  ok(!isInputFocused(hud), "input is not focused");
  is(
    getFilterInput(hud),
    outputScroller.ownerDocument.activeElement,
    "filter input is focused"
  );

  info("Ctrl-U should open view:source when input is focused");
  hud.jsterm.focus();
  const onTabOpen = BrowserTestUtils.waitForNewTab(
    gBrowser,
    url => url.startsWith("view-source:"),
    true
  );
  EventUtils.synthesizeKey("u", { accelKey: true });
  await onTabOpen;
  ok(
    true,
    "The view source tab was opened with the expected keyboard shortcut"
  );
});
