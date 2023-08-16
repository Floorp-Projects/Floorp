/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint no-unused-vars: [2, {"vars": "local"}] */
/* import-globals-from head.js */
/* import-globals-from helper_diff.js */
"use strict";

const beautify = require("resource://devtools/shared/jsbeautify/beautify.js");

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
  evHolder.scrollIntoView({
    block: "center",
    inline: "end",
    behavior: "instant",
  });
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
    const attributes = Array.from(
      header.querySelectorAll(".event-tooltip-attributes")
    );
    const contentBox = header.nextElementSibling;

    info("Looking for " + type.textContent);

    is(type.textContent, expected[i].type, "type matches for " + cssSelector);
    is(
      filename.textContent,
      expected[i].filename,
      "filename matches for " + cssSelector
    );

    Assert.deepEqual(
      attributes.map(el => el.textContent),
      expected[i].attributes,
      `we have the expected attributes for "${cssSelector}"`
    );

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

    const checkbox = header.querySelector("input[type=checkbox]");
    ok(checkbox, "The event toggling checkbox is displayed");
    const disabled = checkbox.hasAttribute("disabled");
    // We can't disable React/jQuery events at the moment, so ensure that for those,
    // the checkbox is disabled.
    const shouldBeDisabled =
      expected[i].attributes?.includes("React") ||
      expected[i].attributes?.includes("jQuery");
    ok(
      disabled === shouldBeDisabled,
      `The checkbox is ${shouldBeDisabled ? "disabled" : "enabled"}\n`
    );

    info(`${label} END`);
  }

  const tooltipHidden = tooltip.once("hidden");
  tooltip.hide();
  await tooltipHidden;
}

/**
 * This should be kept in sync with the content of the window load event listener callback
 * content in doc_markup_events_jquery.html.
 */
function getDocMarkupEventsJQueryLoadHandlerText() {
  return `
          () => {
            const handler1 = function liveDivDblClick() {
              alert(1);
            };
            const handler2 = function liveDivDragStart() {
              alert(2);
            };
            const handler3 = function liveDivDragLeave() {
              alert(3);
            };
            const handler4 = function liveDivDragEnd() {
              alert(4);
            };
            const handler5 = function liveDivDrop() {
              alert(5);
            };
            const handler6 = function liveDivDragOver() {
              alert(6);
            };
            const handler7 = function divClick1() {
              alert(7);
            };
            const handler8 = function divClick2() {
              alert(8);
            };
            const handler9 = function divKeyDown() {
              alert(9);
            };
            const handler10 = function divDragOut() {
              alert(10);
            };

            if ($("#livediv").live) {
              $("#livediv").live("dblclick", handler1);
              $("#livediv").live("dragstart", handler2);
            }

            if ($("#livediv").delegate) {
              $(document).delegate("#livediv", "dragleave", handler3);
              $(document).delegate("#livediv", "dragend", handler4);
            }

            if ($("#livediv").on) {
              $(document).on("drop", "#livediv", handler5);
              $(document).on("dragover", "#livediv", handler6);
              $(document).on("dragout", "#livediv:xxxxx", handler10);
            }

            const div = $("div")[0];
            $(div).click(handler7);
            $(div).click(handler8);
            $(div).keydown(handler9);

            class MyClass {
              constructor() {
                $(document).on("click", '#inclassboundeventdiv', this.onClick.bind(this));
              }
              onClick() { alert(11); }
            }
            new MyClass();
          }`;
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
