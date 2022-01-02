"use strict";

/*
 * This test creates multiple panels, one that has been tagged as specific to its tab's content
 * and one that isn't. When a tab loses focus, panel specific to that tab should close.
 * The non-specific panel should remain open.
 *
 */

add_task(async function() {
  let tab1 = BrowserTestUtils.addTab(gBrowser, "http://mochi.test:8888/#0");
  let tab2 = BrowserTestUtils.addTab(gBrowser, "http://mochi.test:8888/#1");
  let specificPanel = document.createXULElement("panel");
  specificPanel.setAttribute("tabspecific", "true");
  specificPanel.setAttribute("noautohide", "true");
  let generalPanel = document.createXULElement("panel");
  generalPanel.setAttribute("noautohide", "true");
  let anchor = document.getElementById(CustomizableUI.AREA_NAVBAR);

  anchor.appendChild(specificPanel);
  anchor.appendChild(generalPanel);
  is(specificPanel.state, "closed", "specificPanel starts as closed");
  is(generalPanel.state, "closed", "generalPanel starts as closed");

  let specificPanelPromise = BrowserTestUtils.waitForEvent(
    specificPanel,
    "popupshown"
  );
  specificPanel.openPopupAtScreen(210, 210);
  await specificPanelPromise;
  is(specificPanel.state, "open", "specificPanel has been opened");

  let generalPanelPromise = BrowserTestUtils.waitForEvent(
    generalPanel,
    "popupshown"
  );
  generalPanel.openPopupAtScreen(510, 510);
  await generalPanelPromise;
  is(generalPanel.state, "open", "generalPanel has been opened");

  gBrowser.tabContainer.advanceSelectedTab(-1, true);
  is(
    specificPanel.state,
    "closed",
    "specificPanel panel is closed after its tab loses focus"
  );
  is(generalPanel.state, "open", "generalPanel is still open after tab switch");

  specificPanel.remove();
  generalPanel.remove();
  gBrowser.removeTab(tab1);
  gBrowser.removeTab(tab2);
});
