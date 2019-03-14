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
       dimensions. If we don't wait here we could read the monitor image
       URL before it's changed to the widescreen version */
    await TestUtils.waitForTick();

    const img = win.document.getElementById("monitor");
    const measure = new Image();
    const measureLoad = BrowserTestUtils.waitForEvent(measure, "load");
    measure.src =
      getComputedStyle(img).listStyleImage.slice(4, -1).replace(/"/g, "");
    await measureLoad;

    Assert.equal(img.clientWidth, measure.naturalWidth, "Monitor image correct width");
    Assert.equal(img.clientHeight, measure.naturalHeight, "Monitor image correct height");

    win.close();

    await menuClosed;
  });
});
