/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Check whether the preview image for setDesktopBackground is rendered
 * correctly, without stretching
 */

add_task(async function() {
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: "about:logo",
  }, async (browser) => {
    const dialogLoad = BrowserTestUtils.domWindowOpened(
      null,
      async win => {
        await BrowserTestUtils.waitForEvent(win, "load");
        Assert.equal(
          win.document.documentElement.getAttribute("windowtype"),
          "Shell:SetDesktopBackground",
          "Opened correct window"
        );
        return true;
      }
    );

    const image = content.document.images[0];
    EventUtils.synthesizeMouseAtCenter(image, { type: "contextmenu" });

    const menu = document.getElementById("contentAreaContextMenu");
    await BrowserTestUtils.waitForPopupEvent(menu, "shown");
    document.getElementById("context-setDesktopBackground").click();

    // Need to explicitly close the menu (and wait for it), otherwise it fails
    // verify/later tests
    const menuClosed = BrowserTestUtils.waitForPopupEvent(menu, "hidden");
    menu.hidePopup();

    const win = await dialogLoad;

    /* setDesktopBackground.js does a setTimeout to wait for correct
       dimensions. If we don't wait here we could read the preview dimensions
       before they're changed to match the screen */
    await TestUtils.waitForTick();

    const canvas = win.document.getElementById("screen");
    // Only test to two decimal places
    const screenRatio = Math.floor((screen.width / screen.height) * 100);
    const previewRatio = Math.floor((canvas.clientWidth / canvas.clientHeight) * 100);

    Assert.equal(previewRatio, screenRatio, "Preview's aspect ratio matches screen's");

    win.close();

    await menuClosed;
  });
});
