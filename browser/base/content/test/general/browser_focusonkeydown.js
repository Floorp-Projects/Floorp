add_task(async function () {
  let keyUps = 0;

  await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "data:text/html,<body>"
  );

  gURLBar.focus();

  window.addEventListener(
    "keyup",
    function (event) {
      if (event.originalTarget == gURLBar.inputField) {
        keyUps++;
      }
    },
    { capture: true, once: true }
  );

  gURLBar.addEventListener(
    "keydown",
    function (event) {
      gBrowser.selectedBrowser.focus();
    },
    { capture: true, once: true }
  );

  EventUtils.sendString("v");

  is(keyUps, 1, "Key up fired at url bar");

  gBrowser.removeCurrentTab();
});
