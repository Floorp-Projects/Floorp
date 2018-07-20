/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that all of buttons of tool tab go to the overflowed menu when the devtool's
// width is narrow.

const SIDEBAR_WIDTH_PREF = "devtools.toolbox.sidebar.width";

const { Toolbox } = require("devtools/client/framework/toolbox");

add_task(async function(pickerEnable, commandsEnable) {
  // 74px is Chevron(26px) + Meatball(24px) + Close(24px)
  // devtools-browser.css defined this minimum width by using min-width.
  Services.prefs.setIntPref(SIDEBAR_WIDTH_PREF, 74);
  registerCleanupFunction(function() {
    Services.prefs.clearUserPref(SIDEBAR_WIDTH_PREF);
  });
  const tab = await addTab("about:blank");

  info("Open devtools on the Inspector in a side dock");
  const toolbox = await openToolboxForTab(tab, "inspector", Toolbox.HostType.RIGHT);
  await waitUntil(() => toolbox.doc.querySelector(".tools-chevron-menu"));

  await openChevronMenu(toolbox);

  // Check that all of tools is overflowed.
  toolbox.panelDefinitions.forEach(({id}) => {
    const menuItem = toolbox.doc.getElementById("tools-chevron-menupopup-" + id);
    const tab = toolbox.doc.getElementById("toolbox-tab-" + id);
    ok(menuItem, id + " is in the overflowed menu");
    ok(!tab, id + " tab does not exist");
  });

  await closeChevronMenu(toolbox);
});
