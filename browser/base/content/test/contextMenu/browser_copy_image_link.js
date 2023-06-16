/**
 * Testcase for bug 1719203
 * <https://bugzilla.mozilla.org/show_bug.cgi?id=1719203>
 *
 * Load firebird.png, redirect it to doggy.png, and verify that "Copy Image
 * Link" copies firebird.png.
 */

add_task(async function () {
  // This URL will redirect to doggy.png.
  const URL_FIREBIRD =
    "http://mochi.test:8888/browser/browser/base/content/test/contextMenu/firebird.png";

  await BrowserTestUtils.withNewTab(URL_FIREBIRD, async function (browser) {
    // Click image to show context menu.
    let popupShownPromise = BrowserTestUtils.waitForEvent(
      document,
      "popupshown"
    );
    await BrowserTestUtils.synthesizeMouseAtCenter(
      "img",
      { type: "contextmenu", button: 2 },
      browser
    );
    await popupShownPromise;

    await SimpleTest.promiseClipboardChange(URL_FIREBIRD, () => {
      document.getElementById("context-copyimage").doCommand();
    });

    // Close context menu.
    let contextMenu = document.getElementById("contentAreaContextMenu");
    let popupHiddenPromise = BrowserTestUtils.waitForEvent(
      contextMenu,
      "popuphidden"
    );
    contextMenu.hidePopup();
    await popupHiddenPromise;
  });
});
