ChromeUtils.import("resource:///modules/SitePermissions.jsm", this);

async function openTabContextMenu() {
  let contextMenu = document.getElementById("tabContextMenu");
  let popupShownPromise = BrowserTestUtils.waitForEvent(contextMenu, "popupshown");
  EventUtils.synthesizeMouseAtCenter(gBrowser.selectedTab, {type: "contextmenu", button: 2});
  await popupShownPromise;
}
