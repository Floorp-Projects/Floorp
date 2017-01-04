add_task(function *() {
  let keyUps = 0;

  yield BrowserTestUtils.openNewForegroundTab(gBrowser, "data:text/html,<body>");

  gURLBar.focus();

  window.addEventListener("keyup", function countKeyUps(event) {
    window.removeEventListener("keyup", countKeyUps, true);
    if (event.originalTarget == gURLBar.inputField) {
      keyUps++;
    }
  }, true);

  gURLBar.addEventListener("keydown", function redirectFocus(event) {
    gURLBar.removeEventListener("keydown", redirectFocus, true);
    gBrowser.selectedBrowser.focus();
  }, true);

  EventUtils.synthesizeKey("v", { });

  is(keyUps, 1, "Key up fired at url bar");

  gBrowser.removeCurrentTab();
});
