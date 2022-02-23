/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint no-unused-vars: [2, {"vars": "local"}] */
/* import-globals-from head.js */
/* import-globals-from helper_diff.js */
"use strict";

const beautify = require("devtools/shared/jsbeautify/beautify");

loadHelperScript("helper_diff.js");

/**
 * Generator function that runs checkEventsForNode() for each object in the
 * TEST_DATA array.
 */
async function runEventPopupTests(url, tests) {
  const { inspector } = await openInspectorForURL(url);

  await inspector.markup.expandAll();

  for (const test of tests) {
    await checkEventsForNode(test, inspector);
  }

  // Wait for promises to avoid leaks when running this as a single test.
  // We need to do this because we have opened a bunch of popups and don't them
  // to affect other test runs when they are GCd.
  await promiseNextTick();
}

/**
 * Generator function that takes a selector and expected results and returns
 * the event info.
 *
 * @param {Object} test
 *  A test object should contain the following properties:
 *        - selector {String} a css selector targeting the node to edit
 *        - expected {Array} array of expected event objects
 *          - type {String} event type
 *          - filename {String} filename:line where the evt handler is defined
 *          - attributes {Array} array of event attributes ({String})
 *          - handler {String} string representation of the handler
 *        - beforeTest {Function} (optional) a function to execute on the page
 *        before running the test
 *        - isSourceMapped {Boolean} (optional) true if the location
 *        is source-mapped, requiring some extra delay before the checks
 * @param {InspectorPanel} inspector The instance of InspectorPanel currently
 * opened
 */
async function checkEventsForNode(test, inspector) {
  const { selector, expected, beforeTest, isSourceMapped } = test;
  const container = await getContainerForSelector(selector, inspector);

  if (typeof beforeTest === "function") {
    await beforeTest(inspector);
  }

  const evHolder = container.elt.querySelector(
    ".inspector-badge.interactive[data-event]"
  );

  if (expected.length === 0) {
    // If no event is expected, check that event bubble is hidden.
    ok(!evHolder, "event bubble should be hidden");
    return;
  }

  const tooltip = inspector.markup.eventDetailsTooltip;

  await selectNode(selector, inspector);

  let sourceMapPromise = null;
  if (isSourceMapped) {
    sourceMapPromise = tooltip.once("event-tooltip-source-map-ready");
  }

  // Click button to show tooltip
  info("Clicking evHolder");
  evHolder.scrollIntoView();
  EventUtils.synthesizeMouseAtCenter(
    evHolder,
    {},
    inspector.markup.doc.defaultView
  );
  await tooltip.once("shown");
  info("tooltip shown");

  if (isSourceMapped) {
    info("Waiting for source map to be applied");
    await sourceMapPromise;
  }

  // Check values
  const headers = tooltip.panel.querySelectorAll(".event-header");
  const nodeFront = container.node;
  const cssSelector = nodeFront.nodeName + "#" + nodeFront.id;

  for (let i = 0; i < headers.length; i++) {
    const label = `${cssSelector}.${expected[i].type} (index ${i})`;
    info(`${label} START`);

    const header = headers[i];
    const type = header.querySelector(".event-tooltip-event-type");
    const filename = header.querySelector(".event-tooltip-filename");
    const attributes = header.querySelectorAll(".event-tooltip-attributes");
    const contentBox = header.nextElementSibling;

    info("Looking for " + type.textContent);

    is(type.textContent, expected[i].type, "type matches for " + cssSelector);
    is(
      filename.textContent,
      expected[i].filename,
      "filename matches for " + cssSelector
    );

    is(
      attributes.length,
      expected[i].attributes.length,
      "we have the correct number of attributes"
    );

    for (let j = 0; j < expected[i].attributes.length; j++) {
      is(
        attributes[j].textContent,
        expected[i].attributes[j],
        "attribute[" + j + "] matches for " + cssSelector
      );
    }

    is(
      header.classList.contains("content-expanded"),
      false,
      "We are not in expanded state"
    );

    // Make sure the header is not hidden by scrollbars before clicking.
    header.scrollIntoView();

    // Avoid clicking the header's center (could hit the debugger button)
    EventUtils.synthesizeMouse(header, 2, 2, {}, type.ownerGlobal);
    await tooltip.once("event-tooltip-ready");

    is(
      header.classList.contains("content-expanded") &&
        contentBox.hasAttribute("open"),
      true,
      "We are in expanded state and icon changed"
    );

    is(
      tooltip.panel.querySelectorAll(".event-header.content-expanded")
        .length === 1 &&
        tooltip.panel.querySelectorAll(".event-tooltip-content-box[open]")
          .length === 1,
      true,
      "Only one event box is expanded at a time"
    );

    const editor = tooltip.eventTooltip._eventEditors.get(contentBox).editor;
    const tidiedHandler = beautify.js(expected[i].handler, {
      indent_size: 2,
    });
    testDiff(
      editor.getText(),
      tidiedHandler,
      "handler matches for " + cssSelector,
      ok
    );

    info(`${label} END`);
  }

  const tooltipHidden = tooltip.once("hidden");
  tooltip.hide();
  await tooltipHidden;
}

/**
 * Create diff of two strings.
 *
 * @param  {String} text1
 *         String to compare with text2.
 * @param  {String} text2 [description]
 *         String to compare with text1.
 * @param  {String} msg
 *         Message to display on failure. A diff will be displayed after this
 *         message.
 */
function testDiff(text1, text2, msg) {
  let out = "";

  if (text1 === text2) {
    ok(true, msg);
    return;
  }

  const result = textDiff(text1, text2);

  for (const { atom, operation } of result) {
    switch (operation) {
      case "add":
        out += "+ " + atom + "\n";
        break;
      case "delete":
        out += "- " + atom + "\n";
        break;
      case "none":
        out += "  " + atom + "\n";
        break;
    }
  }

  ok(false, msg + "\nDIFF:\n==========\n" + out + "==========\n");
}
