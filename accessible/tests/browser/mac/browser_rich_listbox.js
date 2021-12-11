/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/attributes.js */
loadScripts({ name: "attributes.js", dir: MOCHITESTS_DIR });

addAccessibleTask(
  "mac/doc_rich_listbox.xhtml",
  async (browser, accDoc) => {
    const categories = getNativeInterface(accDoc, "categories");
    const categoriesChildren = categories.getAttributeValue("AXChildren");
    is(categoriesChildren.length, 4, "Found listbox and 4 items");

    const general = getNativeInterface(accDoc, "general");
    is(
      general.getAttributeValue("AXTitle"),
      "general",
      "general has appropriate title"
    );
    is(
      categoriesChildren[0].getAttributeValue("AXTitle"),
      general.getAttributeValue("AXTitle"),
      "Found general listitem"
    );
    is(
      general.getAttributeValue("AXEnabled"),
      1,
      "general is enabled, not dimmed"
    );

    const home = getNativeInterface(accDoc, "home");
    is(home.getAttributeValue("AXTitle"), "home", "home has appropriate title");
    is(
      categoriesChildren[1].getAttributeValue("AXTitle"),
      home.getAttributeValue("AXTitle"),
      "Found home listitem"
    );
    is(home.getAttributeValue("AXEnabled"), 1, "Home is enabled, not dimmed");

    const search = getNativeInterface(accDoc, "search");
    is(
      search.getAttributeValue("AXTitle"),
      "search",
      "search has appropriate title"
    );
    is(
      categoriesChildren[2].getAttributeValue("AXTitle"),
      search.getAttributeValue("AXTitle"),
      "Found search listitem"
    );
    is(
      search.getAttributeValue("AXEnabled"),
      1,
      "search is enabled, not dimmed"
    );

    const privacy = getNativeInterface(accDoc, "privacy");
    is(
      privacy.getAttributeValue("AXTitle"),
      "privacy",
      "privacy has appropriate title"
    );
    is(
      categoriesChildren[3].getAttributeValue("AXTitle"),
      privacy.getAttributeValue("AXTitle"),
      "Found privacy listitem"
    );
  },
  { topLevel: false, chrome: true }
);
