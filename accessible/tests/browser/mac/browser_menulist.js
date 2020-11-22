/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/attributes.js */
/* import-globals-from ../../mochitest/role.js */
/* import-globals-from ../../mochitest/states.js */
loadScripts(
  { name: "role.js", dir: MOCHITESTS_DIR },
  { name: "states.js", dir: MOCHITESTS_DIR },
  { name: "attributes.js", dir: MOCHITESTS_DIR }
);

addAccessibleTask(
  "mac/doc_menulist.xhtml",
  async (browser, accDoc) => {
    const menulist = getNativeInterface(accDoc, "defaultZoom");

    let actions = menulist.actionNames;
    ok(actions.includes("AXPress"), "menu has press action");

    let event = waitForMacEvent("AXMenuOpened");
    menulist.performAction("AXPress");
    const menupopup = await event;

    const menuItems = menupopup.getAttributeValue("AXChildren");
    is(menuItems.length, 4, "Found four children in menulist");
    is(
      menuItems[0].getAttributeValue("AXTitle"),
      "50%",
      "First item has correct title"
    );
    is(
      menuItems[1].getAttributeValue("AXTitle"),
      "100%",
      "Second item has correct title"
    );
    is(
      menuItems[2].getAttributeValue("AXTitle"),
      "150%",
      "Third item has correct title"
    );
    is(
      menuItems[3].getAttributeValue("AXTitle"),
      "200%",
      "Fourth item has correct title"
    );
  },
  { topLevel: false, chrome: true }
);
