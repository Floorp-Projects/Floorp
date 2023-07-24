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

ChromeUtils.defineESModuleGetters(this, {
  PlacesTestUtils: "resource://testing-common/PlacesTestUtils.sys.mjs",
});

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

    // eslint-disable-next-line @microsoft/sdl/no-insecure-url
    await PlacesTestUtils.addVisits(["http://www.example.com/"]);

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

    let onRecreation = waitForEvent(EVENT_SHOW, "link1");
    await SpecialPowers.spawn(browser, [], () => {
      content.document.getElementById("link1").removeAttribute("href");
    });
    await onRecreation;
    link1 = getNativeInterface(accDoc, "link1");
    is(
      link1.getAttributeValue("AXRole"),
      "AXGroup",
      "<a> stripped from href gets group role"
    );

    onRecreation = waitForEvent(EVENT_SHOW, "link2");
    await SpecialPowers.spawn(browser, [], () => {
      content.document.getElementById("link2").removeAttribute("onclick");
    });
    await onRecreation;
    link2 = getNativeInterface(accDoc, "link2");
    is(
      link2.getAttributeValue("AXRole"),
      "AXGroup",
      "<a> stripped from onclick gets group role"
    );

    onRecreation = waitForEvent(EVENT_SHOW, "link3");
    await SpecialPowers.spawn(browser, [], () => {
      content.document
        .getElementById("link3")
        // eslint-disable-next-line @microsoft/sdl/no-insecure-url
        .setAttribute("href", "http://example.com");
    });
    await onRecreation;
    link3 = getNativeInterface(accDoc, "link3");
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

/**
 * Test anchors and linked ui elements attr
 */
addAccessibleTask(
  `
  <a id="link0" href="http://example.com">I am a link</a>
  <a id="link1" href="#">I am a link with an empty anchor</a>
  <a id="link2" href="#hello">I am a link with no corresponding element</a>
  <a id="link3" href="#world">I am a link with a corresponding element</a>
  <a id="link4" href="#empty">I jump to an empty element</a>
  <a id="link5" href="#namedElem">I jump to a named element</a>
  <a id="link6" href="#emptyNamed">I jump to an empty named element</a>
  <h1 id="world">I am that element</h1>
  <h2 id="empty"></h2>
  <a name="namedElem">I have a name</a>
  <a name="emptyNamed"></a>
  <h3>I have no name and no ID</h3>
  <h4></h4>
  `,
  async (browser, accDoc) => {
    let link0 = getNativeInterface(accDoc, "link0");
    let link1 = getNativeInterface(accDoc, "link1");
    let link2 = getNativeInterface(accDoc, "link2");
    let link3 = getNativeInterface(accDoc, "link3");
    let link4 = getNativeInterface(accDoc, "link4");
    let link5 = getNativeInterface(accDoc, "link5");
    let link6 = getNativeInterface(accDoc, "link6");

    is(
      link0.getAttributeValue("AXLinkedUIElements").length,
      0,
      "Link 0 has no linked UI elements"
    );
    is(
      link1.getAttributeValue("AXLinkedUIElements").length,
      0,
      "Link 1 has no linked UI elements"
    );
    is(
      link2.getAttributeValue("AXLinkedUIElements").length,
      0,
      "Link 2 has no linked UI elements"
    );
    is(
      link3.getAttributeValue("AXLinkedUIElements").length,
      1,
      "Link 3 has one linked UI element"
    );
    is(
      link3
        .getAttributeValue("AXLinkedUIElements")[0]
        .getAttributeValue("AXTitle"),
      "I am that element",
      "Link 3 is linked to the heading"
    );
    is(
      link4.getAttributeValue("AXLinkedUIElements").length,
      1,
      "Link 4 has one linked UI element"
    );
    is(
      link4
        .getAttributeValue("AXLinkedUIElements")[0]
        .getAttributeValue("AXTitle"),
      null,
      "Link 4 is linked to the heading"
    );
    is(
      link5.getAttributeValue("AXLinkedUIElements").length,
      1,
      "Link 5 has one linked UI element"
    );
    is(
      link5
        .getAttributeValue("AXLinkedUIElements")[0]
        .getAttributeValue("AXTitle"),
      "",
      "Link 5 is linked to a named element"
    );
    is(
      link6.getAttributeValue("AXLinkedUIElements").length,
      1,
      "Link 6 has one linked UI element"
    );
    is(
      link6
        .getAttributeValue("AXLinkedUIElements")[0]
        .getAttributeValue("AXTitle"),
      "",
      "Link 6 is linked to an empty named element"
    );
  }
);
