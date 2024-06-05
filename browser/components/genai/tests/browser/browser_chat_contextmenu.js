/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Bug 1895789 to standarize contextmenu helpers in BrowserTestUtils
async function openContextMenu() {
  const contextMenu = document.getElementById("contentAreaContextMenu");
  const promise = BrowserTestUtils.waitForEvent(contextMenu, "popupshown");
  await BrowserTestUtils.synthesizeMouse(
    null,
    0,
    0,
    { type: "contextmenu" },
    gBrowser.selectedBrowser
  );
  await promise;
}

async function hideContextMenu() {
  const contextMenu = document.getElementById("contentAreaContextMenu");
  const promise = BrowserTestUtils.waitForEvent(contextMenu, "popuphidden");
  contextMenu.hidePopup();
  await promise;
}

/**
 * Check that the chat context menu is hidden by default
 */
add_task(async function test_hidden_menu() {
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    await openContextMenu();
    Assert.ok(
      document.getElementById("context-ask-chat").hidden,
      "Ask chat menu is hidden"
    );
    await hideContextMenu();
  });
});
