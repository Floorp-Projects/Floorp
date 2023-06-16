/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Test the keyboard shortcuts modal
 */

"use strict";

add_task(async function () {
  const dbg = await initDebuggerWithAbsoluteURL(
    "data:text/html,Test keyboard shortcuts modal"
  );

  ok(!getShortcutsModal(dbg), "The shortcuts modal is hidden by default");

  info("Open the modal with the shortcut");
  pressKeyboardShortcut(dbg);

  const el = await waitFor(() => getShortcutsModal(dbg));
  const sections = [...el.querySelectorAll(".shortcuts-section h2")].map(
    h2 => h2.textContent
  );
  is(
    JSON.stringify(sections),
    JSON.stringify(["Editor", "Stepping", "Search"]),
    "The modal has the expected sections"
  );

  info("Close the modal with the shortcut");
  pressKeyboardShortcut(dbg);
  await waitFor(() => !getShortcutsModal(dbg));
  ok(true, "The modal was closed");
});

function getShortcutsModal(dbg) {
  return findElementWithSelector(dbg, ".shortcuts-modal");
}

function pressKeyboardShortcut(dbg) {
  EventUtils.synthesizeKey(
    "/",
    {
      [Services.appinfo.OS === "Darwin" ? "metaKey" : "ctrlKey"]: true,
    },
    dbg.win
  );
}
