/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* global PanelUI */

let gAppMenuStrings = new Localization(
  ["branding/brand.ftl", "browser/appmenu.ftl"],
  true
);

const CLONED_ATTRS = ["command", "oncommand", "onclick", "key", "disabled"];

/**
 * Tests that the Help panel inside of the AppMenu properly clones
 * the items from the Help menupopup. Also ensures that the AppMenu
 * string variants for those menuitems exist inside of appmenu.ftl.
 */
add_task(async function test_help_panel_cloning() {
  await gCUITestUtils.openMainMenu();
  registerCleanupFunction(async () => {
    await gCUITestUtils.hideMainMenu();
  });

  // Showing the Help panel should be enough to get the menupopup to
  // populate itself.
  let anchor = document.getElementById("PanelUI-menu-button");
  PanelUI.showHelpView(anchor);

  let appMenuHelpSubview = document.getElementById("PanelUI-helpView");
  await BrowserTestUtils.waitForEvent(appMenuHelpSubview, "ViewShowing");

  let helpMenuPopup = document.getElementById("menu_HelpPopup");
  let helpMenuPopupItems = helpMenuPopup.querySelectorAll("menuitem");

  for (let helpMenuPopupItem of helpMenuPopupItems) {
    if (helpMenuPopupItem.hidden) {
      continue;
    }

    let appMenuHelpId = "appMenu_" + helpMenuPopupItem.id;
    info(`Checking ${appMenuHelpId}`);

    let appMenuHelpItem = appMenuHelpSubview.querySelector(`#${appMenuHelpId}`);
    Assert.ok(appMenuHelpItem, "Should have found a cloned AppMenu help item");

    let appMenuHelpItemL10nId = appMenuHelpItem.dataset.l10nId;
    // There is a convention that the Help menu item should have an
    // appmenu-data-l10n-id attribute set as the AppMenu-specific localization
    // id.
    Assert.equal(
      helpMenuPopupItem.getAttribute("appmenu-data-l10n-id"),
      appMenuHelpItemL10nId,
      "Help menuitem supplied a data-l10n-id for the AppMenu Help item"
    );

    let [strings] = gAppMenuStrings.formatMessagesSync([
      { id: appMenuHelpItemL10nId },
    ]);
    Assert.ok(strings, "Should have found strings for the AppMenu help item");

    // Make sure the CLONED_ATTRs are actually cloned.
    for (let attr of CLONED_ATTRS) {
      if (attr == "oncommand" && helpMenuPopupItem.hasAttribute("command")) {
        // If the original element had a "command" attribute set, then the
        // cloned element will have its "oncommand" attribute set to equal
        // the "oncommand" attribute of the <command> pointed to via the
        // original's "command" attribute once it is inserted into the DOM.
        //
        // This is by virtue of the broadcasting ability of XUL <command>
        // elements.
        let commandNode = document.getElementById(
          helpMenuPopupItem.getAttribute("command")
        );
        Assert.equal(
          commandNode.getAttribute("oncommand"),
          appMenuHelpItem.getAttribute("oncommand"),
          "oncommand was properly cloned."
        );
      } else {
        Assert.equal(
          helpMenuPopupItem.getAttribute(attr),
          appMenuHelpItem.getAttribute(attr),
          `${attr} attribute was cloned.`
        );
      }
    }
  }
});
