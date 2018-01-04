// This test ensures that only one command update happens when switching tabs.

"use strict";

add_task(async function() {
  const uri = "data:text/html,<body><input>";
  let tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, uri);
  let tab2 = await BrowserTestUtils.openNewForegroundTab(gBrowser, uri);

  let updates = [];
  function countUpdates(event) { updates.push((new Error()).stack); }
  let updater = document.getElementById("editMenuCommandSetAll");
  updater.addEventListener("commandupdate", countUpdates, true);
  await BrowserTestUtils.switchTab(gBrowser, tab1);

  is(updates.length, 1, "only one command update per tab switch");
  if (updates.length > 1) {
    for (let stack of updates) {
      info("Update stack:\n" + stack);
    }
  }

  updater.removeEventListener("commandupdate", countUpdates, true);
  await BrowserTestUtils.removeTab(tab1);
  await BrowserTestUtils.removeTab(tab2);
});
