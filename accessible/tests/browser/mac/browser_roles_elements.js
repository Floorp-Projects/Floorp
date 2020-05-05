/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/role.js */
/* import-globals-from ../../mochitest/states.js */
loadScripts(
  { name: "role.js", dir: MOCHITESTS_DIR },
  { name: "states.js", dir: MOCHITESTS_DIR }
);

/**
 * Test different HTML elements for their roles and subroles
 */
function testRoleAndSubRole(accDoc, id, axRole, axSubRole) {
  let el = getNativeInterface(accDoc, id);
  if (axRole) {
    is(
      el.getAttributeValue("AXRole"),
      axRole,
      "AXRole for " + id + " is " + axRole
    );
  }
  if (axSubRole) {
    is(
      el.getAttributeValue("AXSubrole"),
      axSubRole,
      "Subrole for " + id + " is " + axSubRole
    );
  }
}

addAccessibleTask(
  `
  <!-- WAI-ARIA landmark roles -->
  <div id="application" role="application"></div>
  <div id="banner" role="banner"></div>
  <div id="complementary" role="complementary"></div>
  <div id="contentinfo" role="contentinfo"></div>
  <div id="form" role="form"></div>
  <div id="main" role="main"></div>
  <div id="navigation" role="navigation"></div>
  <div id="search" role="search"></div>
  <div id="searchbox" role="searchbox"></div>

  <!-- DPub landmarks -->
  <div id="dPubNavigation" role="doc-index"></div>
  <div id="dPubRegion" role="doc-introduction"></div>

  <!-- Other WAI-ARIA widget roles -->
  <div id="alert" role="alert"></div>
  <div id="alertdialog" role="alertdialog"></div>
  <div id="article" role="article"></div>
  <div id="code" role="code"></div>
  <div id="dialog" role="dialog"></div>
  <div id="ariaDocument" role="document"></div>
  <div id="log" role="log"></div>
  <div id="marquee" role="marquee"></div>
  <div id="ariaMath" role="math"></div>
  <div id="note" role="note"></div>
  <div id="ariaRegion" aria-label="region" role="region"></div>
  <div id="ariaStatus" role="status"></div>
  <div id="switch" role="switch"></div>
  <div id="timer" role="timer"></div>
  <div id="tooltip" role="tooltip"></div>

  <!-- True HTML5 search box -->
  <input type="search" id="htmlSearch" />

  <!-- A button morphed into a toggle via ARIA -->
  <button id="toggle" aria-pressed="false"></button>

  <!-- Other elements -->
  <del id="deletion">Deleted text</del>
  <dl id="dl"><dt id="dt">term</dt><dd id="dd">definition</dd></dl>
  <hr id="hr" />
  <ins id="insertion">Inserted text</ins>`,
  (browser, accDoc) => {
    // WAI-ARIA landmark subroles, regardless of AXRole
    testRoleAndSubRole(accDoc, "application", null, "AXLandmarkApplication");
    testRoleAndSubRole(accDoc, "banner", null, "AXLandmarkBanner");
    testRoleAndSubRole(
      accDoc,
      "complementary",
      null,
      "AXLandmarkComplementary"
    );
    testRoleAndSubRole(accDoc, "contentinfo", null, "AXLandmarkContentInfo");
    testRoleAndSubRole(accDoc, "form", null, "AXLandmarkForm");
    testRoleAndSubRole(accDoc, "main", null, "AXLandmarkMain");
    testRoleAndSubRole(accDoc, "navigation", null, "AXLandmarkNavigation");
    testRoleAndSubRole(accDoc, "search", null, "AXLandmarkSearch");
    testRoleAndSubRole(accDoc, "searchbox", null, "AXSearchField");

    // DPub roles map into two categories, sample one of each
    testRoleAndSubRole(
      accDoc,
      "dPubNavigation",
      "AXGroup",
      "AXLandmarkNavigation"
    );
    testRoleAndSubRole(accDoc, "dPubRegion", "AXGroup", "AXLandmarkRegion");

    // ARIA widget roles
    testRoleAndSubRole(accDoc, "alert", null, "AXApplicationAlert");
    testRoleAndSubRole(accDoc, "alertdialog", null, "AXApplicationAlertDialog");
    testRoleAndSubRole(accDoc, "article", null, "AXDocumentArticle");
    testRoleAndSubRole(accDoc, "code", "AXGroup", "AXCodeStyleGroup");
    testRoleAndSubRole(accDoc, "dialog", null, "AXApplicationDialog");
    testRoleAndSubRole(accDoc, "ariaDocument", null, "AXDocument");
    testRoleAndSubRole(accDoc, "log", null, "AXApplicationLog");
    testRoleAndSubRole(accDoc, "marquee", null, "AXApplicationMarquee");
    testRoleAndSubRole(accDoc, "ariaMath", null, "AXDocumentMath");
    testRoleAndSubRole(accDoc, "note", null, "AXDocumentNote");
    testRoleAndSubRole(accDoc, "ariaRegion", null, "AXLandmarkRegion");
    testRoleAndSubRole(accDoc, "ariaStatus", null, "AXApplicationStatus");
    testRoleAndSubRole(accDoc, "switch", "AXCheckBox", "AXSwitch");
    testRoleAndSubRole(accDoc, "timer", null, "AXApplicationTimer");
    testRoleAndSubRole(accDoc, "tooltip", null, "AXUserInterfaceTooltip");

    // True HTML5 search field
    testRoleAndSubRole(accDoc, "htmlSearch", "AXTextField", "AXSearchField");

    // A button morphed into a toggle by ARIA
    testRoleAndSubRole(accDoc, "toggle", "AXCheckBox", "AXToggle");

    // Other elements
    testRoleAndSubRole(accDoc, "deletion", "AXGroup", "AXDeleteStyleGroup");
    testRoleAndSubRole(accDoc, "dl", "AXList", "AXDefinitionList");
    testRoleAndSubRole(accDoc, "dt", "AXGroup", "AXTerm");
    testRoleAndSubRole(accDoc, "dd", "AXGroup", "AXDefinition");
    testRoleAndSubRole(accDoc, "hr", "AXSplitter", "AXContentSeparator");
    testRoleAndSubRole(accDoc, "insertion", "AXGroup", "AXInsertStyleGroup");
  }
);

addAccessibleTask(
  `
  <figure id="figure">
    <img id="img" src="http://example.com/a11y/accessible/tests/mochitest/moz.png" alt="Logo">
    <p>Non-image figure content</p>
    <figcaption id="figcaption">Old Mozilla logo</figcaption>
  </figure>`,
  (browser, accDoc) => {
    let figure = getNativeInterface(accDoc, "figure");
    ok(!figure.getAttributeValue("AXTitle"), "Figure should not have a title");
    is(
      figure.getAttributeValue("AXDescription"),
      "Old Mozilla logo",
      "Correct figure label"
    );
    is(figure.getAttributeValue("AXRole"), "AXGroup", "Correct figure role");
    is(
      figure.getAttributeValue("AXRoleDescription"),
      "figure",
      "Correct figure role description"
    );

    let img = getNativeInterface(accDoc, "img");
    ok(!img.getAttributeValue("AXTitle"), "img should not have a title");
    is(img.getAttributeValue("AXDescription"), "Logo", "Correct img label");
    is(img.getAttributeValue("AXRole"), "AXImage", "Correct img role");
    is(
      img.getAttributeValue("AXRoleDescription"),
      "image",
      "Correct img role description"
    );

    let figcaption = getNativeInterface(accDoc, "figcaption");
    ok(
      !figcaption.getAttributeValue("AXTitle"),
      "figcaption should not have a title"
    );
    ok(
      !figcaption.getAttributeValue("AXDescription"),
      "figcaption should not have a label"
    );
    is(
      figcaption.getAttributeValue("AXRole"),
      "AXGroup",
      "Correct figcaption role"
    );
    is(
      figcaption.getAttributeValue("AXRoleDescription"),
      "group",
      "Correct figcaption role description"
    );
  }
);
