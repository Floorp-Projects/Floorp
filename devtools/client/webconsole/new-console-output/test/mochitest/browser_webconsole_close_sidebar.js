/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that the sidebar is hidden when the console is cleared.

"use strict";

const TEST_URI = "data:text/html;charset=utf8,";

add_task(async function () {
  // Should be removed when sidebar work is complete
  await SpecialPowers.pushPrefEnv({"set": [
    ["devtools.webconsole.sidebarToggle", true]
  ]});

  let hud = await openNewTabAndConsole(TEST_URI);
  await showSidebar(hud);

  info("Click the clear console button");
  let clearButton = hud.ui.document.querySelector(".devtools-button");
  clearButton.click();
  await waitFor(() => findMessages(hud, "").length == 0);
  let sidebar = hud.ui.document.querySelector(".sidebar");
  ok(!sidebar, "Sidebar hidden after clear console button clicked");

  await showSidebar(hud);

  info("Send a console.clear()");
  let onMessagesCleared = waitForMessage(hud, "Console was cleared");
  ContentTask.spawn(gBrowser.selectedBrowser, {}, function () {
    content.wrappedJSObject.console.clear();
  });
  await onMessagesCleared;
  sidebar = hud.ui.document.querySelector(".sidebar");
  ok(!sidebar, "Sidebar hidden after console.clear()");

  await showSidebar(hud);

  info("Send ctrl-l to clear console");
  let clearShortcut;
  if (Services.appinfo.OS === "Darwin") {
    clearShortcut = WCUL10n.getStr("webconsole.clear.keyOSX");
  } else {
    clearShortcut = WCUL10n.getStr("webconsole.clear.key");
  }
  synthesizeKeyShortcut(clearShortcut);
  await waitFor(() => findMessages(hud, "").length == 0);
  sidebar = hud.ui.document.querySelector(".sidebar");
  ok(!sidebar, "Sidebar hidden after console.clear()");
});

async function showSidebar(hud) {
  let toggleButton = hud.ui.document.querySelector(".webconsole-sidebar-button");
  let wrapper = hud.ui.document.querySelector(".webconsole-output-wrapper");
  let onSidebarShown = waitForNodeMutation(wrapper, { childList: true });
  toggleButton.click();
  await onSidebarShown;
}
