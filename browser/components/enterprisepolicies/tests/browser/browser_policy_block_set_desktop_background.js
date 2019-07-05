/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

add_task(async function setup() {
  await setupPolicyEngineWithJson({
    policies: {
      DisableSetDesktopBackground: true,
    },
  });
});

add_task(async function test_check_set_desktop_background() {
  const imageUrl =
    "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAABmJLR0QA/wD/AP+gvaeTAAAACXBIWXMAAAsTAAALEwEAmpwYAAAAB3RJTUUH3gwMDAsTBZbkNwAAAB1pVFh0Q29tbWVudAAAAAAAQ3JlYXRlZCB3aXRoIEdJTVBkLmUHAAABNElEQVQ4y8WSsU0DURBE3yyWIaAJaqAAN4DPSL6AlIACKIEOyJEgRsIgOOkiInJqgAKowNg7BHdn7MOksNl+zZ//dvbDf5cAiklp22BdVtXdeTEpDYDB9m1VzU6OJuVp2NdEQCaI96fH2YHG4+mDduKYNMYINTcjcGbXzQVDEAphG0k48zUsajIbnAiMIXThpW8EICE0RAK4dvoKg9NIcTiQ589otyHOZLnwqK5nLwBFUZ4igc3iM0d1ff8CMC6mZ6Ihiaqq3gi1aUAnArD00SW1fq5OLBg0ymYmSZsR2/t4e/rGyCLW0sbp3oq+yTYqVgytQWui2FS7XYF7GFprY921T4CNQt8zr47dNzCkIX7y/jBtH+v+RGMQrc828W8pApnZbmEVQp/Ae7BlOy2ttib81/UFc+WRWEbjckIAAAAASUVORK5CYII=";
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    imageUrl,
    true
  );

  // Right click on the image and wait for the context menu to open
  let contextMenu = document.getElementById("contentAreaContextMenu");
  let promiseContextMenuOpen = BrowserTestUtils.waitForEvent(
    contextMenu,
    "popupshown"
  );
  await BrowserTestUtils.synthesizeMouse(
    "img",
    0,
    0,
    {
      type: "contextmenu",
      button: 2,
      centered: true,
    },
    gBrowser.selectedBrowser
  );
  await promiseContextMenuOpen;
  info("Context Menu Shown");

  let buttonElement = document.getElementById("context-setDesktopBackground");
  is(
    buttonElement.hidden,
    true,
    'The "Set Desktop Background" context menu element should be hidden'
  );

  let promiseContextMenuHidden = BrowserTestUtils.waitForEvent(
    contextMenu,
    "popuphidden"
  );
  contextMenu.hidePopup();
  await promiseContextMenuHidden;
  BrowserTestUtils.removeTab(tab);
});
