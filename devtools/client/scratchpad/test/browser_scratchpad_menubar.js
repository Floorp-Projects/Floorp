/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Sanity test that menu bar is displayed. If open the scratchpad as toolbox panel,
// this menu should be hidden.

var {TargetFactory} = require("devtools/client/framework/target");

add_task(async function() {
  // Now open the scratchpad as window.
  info("Test existence of menu bar of scratchpad.");
  const options = {
    tabContent: "Sanity test for scratchpad panel."
  };

  info("Open scratchpad.");
  let [win] = await openTabAndScratchpad(options);

  let menuToolbar = win.document.getElementById("sp-menu-toolbar");
  ok(menuToolbar, "The scratchpad should have a menu bar.");
});

add_task(async function() {
  // Now open the scratchpad panel after setting visibility preference.
  info("Test existence of menu bar of scratchpad panel.");
  await new Promise(resolve => {
    SpecialPowers.pushPrefEnv({"set": [["devtools.scratchpad.enabled", true]]}, resolve);
  });

  info("Open devtools on the Scratchpad.");
  let target = TargetFactory.forTab(gBrowser.selectedTab);
  let toolbox = await gDevTools.showToolbox(target, "scratchpad");

  let menuToolbar = toolbox.doc.getElementById("sp-menu-toolbar");
  ok(!menuToolbar, "The scratchpad panel should not have a menu bar.");
});
