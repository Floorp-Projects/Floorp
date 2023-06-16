// Test ability to focus search field by using keyboard
"use strict";

add_task(async function () {
  await openTabAndSetupStorage(MAIN_DOMAIN_SECURED + "storage-search.html");

  gUI.tree.expandAll();
  await selectTreeItem(["localStorage", "https://test1.example.org"]);

  await focusSearchBoxUsingShortcut(gPanelWindow);
  ok(
    containsFocus(gPanelWindow.document, gUI.searchBox),
    "Focus is in a searchbox"
  );
});
