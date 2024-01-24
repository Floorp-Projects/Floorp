/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { AppMenuNotifications } = ChromeUtils.importESModule(
  "resource://gre/modules/AppMenuNotifications.sys.mjs"
);

/**
 * The update banner should become visible when the badge-only notification is
 * shown before opening the menu.
 */
add_task(async function testBannerVisibilityBeforeOpen() {
  let newWin = await BrowserTestUtils.openNewBrowserWindow();

  AppMenuNotifications.showBadgeOnlyNotification("update-restart");

  let menuButton = newWin.document.getElementById("PanelUI-menu-button");
  let shown = BrowserTestUtils.waitForEvent(
    newWin.PanelUI.mainView,
    "ViewShown"
  );
  menuButton.click();
  await shown;

  let banner = newWin.document.getElementById("appMenu-proton-update-banner");

  let labelPromise = BrowserTestUtils.waitForMutationCondition(
    banner,
    { attributes: true, attributeFilter: ["label"] },
    () => banner.hasAttribute("label")
  );

  ok(!banner.hidden, "Update banner should be shown");

  await labelPromise;

  Assert.notEqual(
    banner.getAttribute("label"),
    "",
    "Update banner should contain text"
  );

  AppMenuNotifications.removeNotification(/.*/);

  await BrowserTestUtils.closeWindow(newWin);
});

/**
 * The update banner should become visible when the badge-only notification is
 * shown during the menu is opened.
 */
add_task(async function testBannerVisibilityDuringOpen() {
  let newWin = await BrowserTestUtils.openNewBrowserWindow();

  let menuButton = newWin.document.getElementById("PanelUI-menu-button");
  let shown = BrowserTestUtils.waitForEvent(
    newWin.PanelUI.mainView,
    "ViewShown"
  );
  menuButton.click();
  await shown;

  let banner = newWin.document.getElementById("appMenu-proton-update-banner");
  ok(
    !banner.hasAttribute("label"),
    "Update banner shouldn't contain text before notification"
  );

  let labelPromise = BrowserTestUtils.waitForMutationCondition(
    banner,
    { attributes: true, attributeFilter: ["label"] },
    () => banner.hasAttribute("label")
  );

  AppMenuNotifications.showNotification("update-restart");

  ok(!banner.hidden, "Update banner should be shown");

  await labelPromise;

  Assert.notEqual(
    banner.getAttribute("label"),
    "",
    "Update banner should contain text"
  );

  AppMenuNotifications.removeNotification(/.*/);

  await BrowserTestUtils.closeWindow(newWin);
});

/**
 * The update banner should become visible when the badge-only notification is
 * shown after opening/closing the menu, so that the DOM tree is there but
 * the menu is closed.
 */
add_task(async function testBannerVisibilityAfterClose() {
  let newWin = await BrowserTestUtils.openNewBrowserWindow();

  let menuButton = newWin.document.getElementById("PanelUI-menu-button");
  let shown = BrowserTestUtils.waitForEvent(
    newWin.PanelUI.mainView,
    "ViewShown"
  );
  menuButton.click();
  await shown;

  ok(newWin.PanelUI.mainView.hasAttribute("visible"));

  let banner = newWin.document.getElementById("appMenu-proton-update-banner");

  ok(banner.hidden, "Update banner should be hidden before notification");
  ok(
    !banner.hasAttribute("label"),
    "Update banner shouldn't contain text before notification"
  );

  let labelPromise = BrowserTestUtils.waitForMutationCondition(
    banner,
    { attributes: true, attributeFilter: ["label"] },
    () => banner.hasAttribute("label")
  );

  let hidden = BrowserTestUtils.waitForCondition(() => {
    return !newWin.PanelUI.mainView.hasAttribute("visible");
  });
  menuButton.click();
  await hidden;

  AppMenuNotifications.showBadgeOnlyNotification("update-restart");

  shown = BrowserTestUtils.waitForEvent(newWin.PanelUI.mainView, "ViewShown");
  menuButton.click();
  await shown;

  ok(!banner.hidden, "Update banner should be shown");

  await labelPromise;

  Assert.notEqual(
    banner.getAttribute("label"),
    "",
    "Update banner should contain text"
  );

  AppMenuNotifications.removeNotification(/.*/);

  await BrowserTestUtils.closeWindow(newWin);
});
