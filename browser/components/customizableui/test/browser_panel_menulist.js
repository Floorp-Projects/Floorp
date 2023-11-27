/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const kViewID = "panelview-with-menulist";

/**
 * When there's a menulist inside a panelview, closing it shouldn't close the panel.
 */
add_task(async function test_closing_menulist_should_not_close_panel() {
  let viewCache = document.getElementById("appMenu-viewCache");
  let panelview = document.createXULElement("panelview");
  panelview.id = kViewID;
  let menulist = document.createXULElement("menulist");
  let popup = document.createXULElement("menupopup");
  for (let item of ["one", "two"]) {
    let menuitem = document.createXULElement("menuitem");
    menuitem.id = `menuitem-${item}`;
    menuitem.setAttribute("label", item);
    popup.append(menuitem);
  }
  menulist.append(popup);
  panelview.append(menulist);
  viewCache.append(panelview);
  await PanelUI.showSubView(kViewID, PanelUI.menuButton);
  let panel = panelview.closest("panel");

  // Ensure that not only has the subview started showing, the panel is
  // all the way open:
  await BrowserTestUtils.waitForPopupEvent(panel, "shown");

  registerCleanupFunction(async () => {
    if (panel && panel.state != "closed") {
      let panelGone = BrowserTestUtils.waitForPopupEvent(panel, "hidden");
      panel.hidePopup();
      await panelGone;
    }
    panelview.remove();
  });

  let shown = BrowserTestUtils.waitForPopupEvent(popup, "shown");
  menulist.openMenu(true);
  await shown;
  let hidden = BrowserTestUtils.waitForPopupEvent(popup, "hidden");
  popup.activateItem(popup.firstElementChild);
  await hidden;

  Assert.equal(panel?.state, "open", "Panel should still be open.");
});
