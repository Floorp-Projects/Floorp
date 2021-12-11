add_task(async function() {
  // Even if modifier of a shortcut key same as modifier of content access key,
  // the shortcut key should be executed if (remote) content doesn't handle it.
  // This test uses existing shortcut key declaration on Linux and Windows.
  // If you remove or change Alt + D, you need to keep check this with changing
  // the pref or result check.

  await new Promise(resolve => {
    SpecialPowers.pushPrefEnv(
      {
        set: [
          ["ui.key.generalAccessKey", -1],
          ["ui.key.chromeAccess", 0 /* disabled */],
          ["ui.key.contentAccess", 4 /* Alt */],
          ["browser.search.widget.inNavBar", true],
        ],
      },
      resolve
    );
  });

  const kTestPage = "data:text/html,<body>simple web page</body>";
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, kTestPage);

  let searchBar = BrowserSearch.searchBar;
  searchBar.focus();

  function promiseURLBarHasFocus() {
    return new Promise(resolve => {
      if (gURLBar.focused) {
        ok(true, "The URL bar already has focus");
        resolve();
        return;
      }
      info("Waiting focus event...");
      gURLBar.addEventListener(
        "focus",
        () => {
          ok(true, "The URL bar gets focus");
          resolve();
        },
        { once: true }
      );
    });
  }

  function promiseURLBarSelectsAllText() {
    return new Promise(resolve => {
      function isAllTextSelected() {
        return (
          gURLBar.inputField.selectionStart === 0 &&
          gURLBar.inputField.selectionEnd === gURLBar.inputField.value.length
        );
      }
      if (isAllTextSelected()) {
        ok(true, "All text of the URL bar is already selected");
        isnot(
          gURLBar.inputField.value,
          "",
          "The URL bar should have non-empty text"
        );
        resolve();
        return;
      }
      info("Waiting selection changes...");
      function tryToCheckItLater() {
        if (!isAllTextSelected()) {
          SimpleTest.executeSoon(tryToCheckItLater);
          return;
        }
        ok(true, "All text of the URL bar should be selected");
        isnot(
          gURLBar.inputField.value,
          "",
          "The URL bar should have non-empty text"
        );
        resolve();
      }
      SimpleTest.executeSoon(tryToCheckItLater);
    });
  }

  // Alt + D is a shortcut key to move focus to the URL bar and selects its text.
  info("Pressing Alt + D in the search bar...");
  EventUtils.synthesizeKey("d", { altKey: true });

  await promiseURLBarHasFocus();
  await promiseURLBarSelectsAllText();

  // Alt + D in the URL bar should select all text in it.
  await gURLBar.focus();
  await promiseURLBarHasFocus();
  gURLBar.inputField.selectionStart = gURLBar.inputField.selectionEnd =
    gURLBar.inputField.value.length;

  info("Pressing Alt + D in the URL bar...");
  EventUtils.synthesizeKey("d", { altKey: true });
  await promiseURLBarHasFocus();
  await promiseURLBarSelectsAllText();

  gBrowser.removeCurrentTab();
});
