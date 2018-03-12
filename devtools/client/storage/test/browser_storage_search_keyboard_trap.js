// Test ability to focus search field by using keyboard
"use strict";

add_task(function* () {
  yield openTabAndSetupStorage(MAIN_DOMAIN + "storage-search.html");

  gUI.tree.expandAll();
  yield selectTreeItem(["localStorage", "http://test1.example.org"]);

  yield focusSearchBoxUsingShortcut(gPanelWindow);
  ok(containsFocus(gPanelWindow.document, gUI.searchBox),
     "Focus is in a searchbox");

  yield finishTests();
});
