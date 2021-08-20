/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/*
 * This test creates multiple panels, one that has been tagged as location specific
 * and one that isn't. When the location changes, the specific panel should close.
 * The non-specific panel should remain open.
 *
 */

function synthesizeKeys(input) {
  for (const key of input.split("")) {
    EventUtils.synthesizeKey(key, {});
  }
}

add_task(async function() {
  let specificPanel = document.createXULElement("panel");
  specificPanel.setAttribute("locationspecific", "true");
  specificPanel.setAttribute("noautohide", "true");
  specificPanel.height = "100px";
  specificPanel.width = "100px";

  let generalPanel = document.createXULElement("panel");
  generalPanel.setAttribute("noautohide", "true");
  generalPanel.height = "100px";
  generalPanel.width = "100px";

  let anchor = document.getElementById(CustomizableUI.AREA_NAVBAR);

  anchor.appendChild(specificPanel);
  anchor.appendChild(generalPanel);
  is(specificPanel.state, "closed", "specificPanel starts as closed");
  is(generalPanel.state, "closed", "generalPanel starts as closed");

  let specificPanelPromise = BrowserTestUtils.waitForEvent(
    specificPanel,
    "popupshown"
  );

  specificPanel.openPopupAtScreen(0, 0);

  await specificPanelPromise;
  is(specificPanel.state, "open", "specificPanel has been opened");

  let generalPanelPromise = BrowserTestUtils.waitForEvent(
    generalPanel,
    "popupshown"
  );

  generalPanel.openPopupAtScreen(100, 0);

  await generalPanelPromise;
  is(generalPanel.state, "open", "generalPanel has been opened");

  let specificPanelHiddenPromise = BrowserTestUtils.waitForEvent(
    specificPanel,
    "popuphidden"
  );

  // Trigger a url change through Ctrl+l
  EventUtils.synthesizeKey("l", { ctrlKey: true });
  synthesizeKeys("http://mochi.test:8888/#0");
  EventUtils.synthesizeKey("KEY_Enter", {});

  await specificPanelHiddenPromise;

  is(
    specificPanel.state,
    "closed",
    "specificPanel panel is closed after location change"
  );
  is(
    generalPanel.state,
    "open",
    "generalPanel is still open after location change"
  );

  specificPanel.remove();
  generalPanel.remove();
});
