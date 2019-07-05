/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that the sidebar can be scrolled.

"use strict";

const TEST_URI = `data:text/html;charset=utf8,Test sidebar scroll`;

add_task(async function() {
  // Should be removed when sidebar work is complete
  await pushPref("devtools.webconsole.sidebarToggle", true);
  const isMacOS = Services.appinfo.OS === "Darwin";

  const hud = await openNewTabAndConsole(TEST_URI);

  const onMessage = waitForMessage(hud, "Document");
  ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    content.wrappedJSObject.console.log(content.wrappedJSObject.document);
  });

  const { node } = await onMessage;
  const object = node.querySelector(".object-inspector .node");

  info("Ctrl+click on an object to put it in the sidebar");
  const onSidebarShown = waitFor(() =>
    hud.ui.document.querySelector(".sidebar")
  );
  EventUtils.sendMouseEvent(
    {
      type: "click",
      [isMacOS ? "metaKey" : "ctrlKey"]: true,
    },
    object,
    hud.ui.window
  );
  await onSidebarShown;
  const sidebarContents = hud.ui.document.querySelector(".sidebar-contents");

  // Let's wait until the object is fully expanded.
  await waitFor(() => sidebarContents.querySelectorAll(".node").length > 1);
  ok(
    sidebarContents.scrollHeight > sidebarContents.clientHeight,
    "Sidebar overflows"
  );
});
