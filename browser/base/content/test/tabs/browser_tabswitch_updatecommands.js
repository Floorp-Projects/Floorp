// This test ensures that only one command update happens when switching tabs.

"use strict";

add_task(async function() {
  const uri = "data:text/html,<body><input>";
  let tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, uri);
  let tab2 = await BrowserTestUtils.openNewForegroundTab(gBrowser, uri);

  let updates = 0;
  function countUpdates(event) { updates++; }
  let updater = document.getElementById("editMenuCommandSetAll");
  updater.addEventListener("commandupdate", countUpdates, true);
  await BrowserTestUtils.switchTab(gBrowser, tab1);

  is(updates, 1, "only one command update per tab switch");

  updater.removeEventListener("commandupdate", countUpdates, true);
  await BrowserTestUtils.removeTab(tab1);
  await BrowserTestUtils.removeTab(tab2);
});
