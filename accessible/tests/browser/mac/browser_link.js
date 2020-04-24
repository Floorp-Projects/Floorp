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

ChromeUtils.defineModuleGetter(
  this,
  "PlacesTestUtils",
  "resource://testing-common/PlacesTestUtils.jsm"
);

/**
 * Test visited link properties.
 */
addAccessibleTask(
  `
  <a id="link" href="http://www.example.com/">I am a non-visited link</a><br>
  `,
  async (browser, accDoc) => {
    let link = getNativeInterface(accDoc, "link");
    let stateChanged = waitForEvent(EVENT_STATE_CHANGE, "link");

    is(link.getAttributeValue("AXVisited"), 0, "Link has not been visited");

    PlacesTestUtils.addVisits(["http://www.example.com/"]);

    await stateChanged;
    is(link.getAttributeValue("AXVisited"), 1, "Link has been visited");

    // Ensure history is cleared before running
    await PlacesUtils.history.clear();
  }
);

/**
 * Test linked vs unlinked anchor tags
 */
addAccessibleTask(
  `
  <a id="link1" href="#">I am a link link</a>
  <a id="link2" onclick="console.log('hi')">I am a link-ish link</a>
  <a id="link3">I am a non-link link</a>
  `,
  async (browser, accDoc) => {
    let link1 = getNativeInterface(accDoc, "link1");
    is(
      link1.getAttributeValue("AXRole"),
      "AXLink",
      "a[href] gets correct link role"
    );
    ok(
      link1.attributeNames.includes("AXVisited"),
      "Link has visited attribute"
    );
    ok(link1.attributeNames.includes("AXURL"), "Link has URL attribute");

    let link2 = getNativeInterface(accDoc, "link2");
    is(
      link2.getAttributeValue("AXRole"),
      "AXLink",
      "a[onclick] gets correct link role"
    );
    ok(
      link2.attributeNames.includes("AXVisited"),
      "Link has visited attribute"
    );
    ok(link2.attributeNames.includes("AXURL"), "Link has URL attribute");

    let link3 = getNativeInterface(accDoc, "link3");
    is(
      link3.getAttributeValue("AXRole"),
      "AXGroup",
      "bare <a> gets correct group role"
    );
    ok(
      !link3.attributeNames.includes("AXVisited"),
      "Non-link should not have visited attribute"
    );
    ok(
      !link3.attributeNames.includes("AXURL"),
      "Non-link should not have URL attribute"
    );

    let stateChanged = waitForEvent(EVENT_STATE_CHANGE, "link1");
    await SpecialPowers.spawn(browser, [], () => {
      content.document.getElementById("link1").removeAttribute("href");
    });
    await stateChanged;
    is(
      link1.getAttributeValue("AXRole"),
      "AXGroup",
      "<a> stripped from href gets group role"
    );
    ok(
      !link1.attributeNames.includes("AXVisited"),
      "Non-link should not have visited attribute"
    );
    ok(
      !link1.attributeNames.includes("AXURL"),
      "Non-link should not have URL attribute"
    );

    stateChanged = waitForEvent(EVENT_STATE_CHANGE, "link2");
    await SpecialPowers.spawn(browser, [], () => {
      content.document.getElementById("link2").removeAttribute("onclick");
    });
    await stateChanged;
    is(
      link2.getAttributeValue("AXRole"),
      "AXGroup",
      "<a> stripped from onclick gets group role"
    );
    ok(
      !link2.attributeNames.includes("AXVisited"),
      "Non-link should not have visited attribute"
    );
    ok(
      !link2.attributeNames.includes("AXURL"),
      "Non-link should not have URL attribute"
    );

    stateChanged = waitForEvent(EVENT_STATE_CHANGE, "link3");
    await SpecialPowers.spawn(browser, [], () => {
      content.document
        .getElementById("link3")
        .setAttribute("href", "http://example.com");
    });
    await stateChanged;
    is(
      link3.getAttributeValue("AXRole"),
      "AXLink",
      "href added to bare a gets link role"
    );

    ok(
      link3.attributeNames.includes("AXVisited"),
      "Link has visited attribute"
    );
    ok(link3.attributeNames.includes("AXURL"), "Link has URL attribute");
  }
);
