/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from ../../mochitest/role.js */
loadScripts({ name: "role.js", dir: MOCHITESTS_DIR });

/**
 * Verify that an anchor element reports a generic role without an href
 * attribute and reports a LINK role with it present. Verify that these roles
 * change as the attribute appears and disappears.
 */
addAccessibleTask(
  `
<a id="link">test</a>
  `,
  async function (browser, accDoc) {
    let link = findAccessibleChildByID(accDoc, "link");
    is(link.role, ROLE_TEXT, "Checking role of anchor element without href");

    let onHideAndShow = waitForEvents({
      expected: [
        [EVENT_HIDE, link],
        [EVENT_SHOW, "link"],
      ],
    });
    info("Adding an href to the anchor element");
    await invokeContentTask(browser, [], () => {
      content.document.getElementById("link").setAttribute("href", "#");
    });
    await onHideAndShow;

    link = findAccessibleChildByID(accDoc, "link");
    is(link.role, ROLE_LINK, "Checking role of anchor element with href");

    onHideAndShow = waitForEvents({
      expected: [
        [EVENT_HIDE, link],
        [EVENT_SHOW, "link"],
      ],
    });
    info("Removing the href from the anchor element");
    await invokeContentTask(browser, [], () => {
      content.document.getElementById("link").removeAttribute("href");
    });
    await onHideAndShow;
    link = findAccessibleChildByID(accDoc, "link");
    is(link.role, ROLE_TEXT, "Checking role of anchor element without href");
  },
  {
    chrome: true,
    topLevel: true,
    iframe: true,
    remoteIframe: true,
  }
);

/**
 * Verify that an anchor element reports a generic role without a click listener
 * and reports a LINK role with it present. Verify that these roles change as
 * the click listener appears.
 */
addAccessibleTask(
  `
<a id="link">test</a>
  `,
  async function (browser, accDoc) {
    let link = findAccessibleChildByID(accDoc, "link");
    is(
      link.role,
      ROLE_TEXT,
      "Checking role of anchor element without click listener"
    );

    let onHideAndShow = waitForEvents({
      expected: [
        [EVENT_HIDE, link],
        [EVENT_SHOW, "link"],
      ],
    });
    info("Adding a click listener to the anchor element");
    await invokeContentTask(browser, [], () => {
      content.document
        .getElementById("link")
        .addEventListener("click", () => {});
    });
    await onHideAndShow;

    link = findAccessibleChildByID(accDoc, "link");
    is(
      link.role,
      ROLE_LINK,
      "Checking role of anchor element with click listener"
    );
  },
  {
    chrome: true,
    topLevel: true,
    iframe: true,
    remoteIframe: true,
  }
);

/**
 * Verify that an area element reports a generic role without an href
 * attribute and reports a LINK role with it present. Verify that these roles
 * change as the attribute appears and disappears.
 */
addAccessibleTask(
  `
<map name="map">
  <area id="link">
</map>
<img id="img" usemap="#map" src="http://example.com/a11y/accessible/tests/mochitest/letters.gif">
`,
  async function (browser, accDoc) {
    let link = findAccessibleChildByID(accDoc, "link");
    is(link.role, ROLE_TEXT, "Checking role of area element without href");

    let img = findAccessibleChildByID(accDoc, "img");
    let onHideAndShow = waitForEvents({
      expected: [
        [EVENT_HIDE, img],
        [EVENT_SHOW, "img"],
      ],
    });
    info("Adding an href to the area element");
    await invokeContentTask(browser, [], () => {
      content.document.getElementById("link").setAttribute("href", "#");
    });
    await onHideAndShow;

    link = findAccessibleChildByID(accDoc, "link");
    is(link.role, ROLE_LINK, "Checking role of area element with href");

    img = findAccessibleChildByID(accDoc, "img");
    onHideAndShow = waitForEvents({
      expected: [
        [EVENT_HIDE, img],
        [EVENT_SHOW, "img"],
      ],
    });
    info("Removing the href from the area element");
    await invokeContentTask(browser, [], () => {
      content.document.getElementById("link").removeAttribute("href");
    });
    await onHideAndShow;
    link = findAccessibleChildByID(accDoc, "link");
    is(link.role, ROLE_TEXT, "Checking role of area element without href");
  },
  {
    chrome: true,
    topLevel: true,
    iframe: true,
    remoteIframe: true,
  }
);

/**
 * Verify that an area element reports a generic role without a click listener
 * and reports a LINK role with it present. Verify that these roles change as
 * the click listener appears.
 */
addAccessibleTask(
  `
<map name="map">
  <area id="link">
</map>
<img id="img" usemap="#map" src="http://example.com/a11y/accessible/tests/mochitest/letters.gif">
  `,
  async function (browser, accDoc) {
    let link = findAccessibleChildByID(accDoc, "link");
    is(
      link.role,
      ROLE_TEXT,
      "Checking role of area element without click listener"
    );

    let img = findAccessibleChildByID(accDoc, "img");
    let onHideAndShow = waitForEvents({
      expected: [
        [EVENT_HIDE, img],
        [EVENT_SHOW, "img"],
      ],
    });
    info("Adding a click listener to the area element");
    await invokeContentTask(browser, [], () => {
      content.document
        .getElementById("link")
        .addEventListener("click", () => {});
    });
    await onHideAndShow;

    link = findAccessibleChildByID(accDoc, "link");
    is(
      link.role,
      ROLE_LINK,
      "Checking role of area element with click listener"
    );
  },
  {
    chrome: true,
    topLevel: true,
    iframe: true,
    remoteIframe: true,
  }
);
