/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint no-unused-vars: [2, {"vars": "local"}] */
/* import-globals-from head.js */
"use strict";

/**
 * Generator function that runs checkEventsForNode() for each object in the
 * TEST_DATA array.
 */
function* runEventPopupTests(url, tests) {
  let {inspector, testActor} = yield openInspectorForURL(url);

  yield inspector.markup.expandAll();

  for (let test of tests) {
    yield checkEventsForNode(test, inspector, testActor);
  }

  // Wait for promises to avoid leaks when running this as a single test.
  // We need to do this because we have opened a bunch of popups and don't them
  // to affect other test runs when they are GCd.
  yield promiseNextTick();
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
 * @param {InspectorPanel} inspector The instance of InspectorPanel currently
 * opened
 * @param {TestActorFront} testActor
 */
function* checkEventsForNode(test, inspector, testActor) {
  let {selector, expected, beforeTest} = test;
  let container = yield getContainerForSelector(selector, inspector);

  if (typeof beforeTest === "function") {
    yield beforeTest(inspector, testActor);
  }

  let evHolder = container.elt.querySelector(".markupview-events");

  if (expected.length === 0) {
    // if no event is expected, simply check that the event bubble is hidden
    is(evHolder.style.display, "none", "event bubble should be hidden");
    return;
  }

  let tooltip = inspector.markup.eventDetailsTooltip;

  yield selectNode(selector, inspector);

  // Click button to show tooltip
  info("Clicking evHolder");
  EventUtils.synthesizeMouseAtCenter(evHolder, {},
    inspector.markup.doc.defaultView);
  yield tooltip.once("shown");
  info("tooltip shown");

  // Check values
  let headers = tooltip.content.querySelectorAll(".event-header");
  let nodeFront = container.node;
  let cssSelector = nodeFront.nodeName + "#" + nodeFront.id;

  for (let i = 0; i < headers.length; i++) {
    info("Processing header[" + i + "] for " + cssSelector);

    let header = headers[i];
    let type = header.querySelector(".event-tooltip-event-type");
    let filename = header.querySelector(".event-tooltip-filename");
    let attributes = header.querySelectorAll(".event-tooltip-attributes");
    let contentBox = header.nextElementSibling;

    is(type.getAttribute("value"), expected[i].type,
       "type matches for " + cssSelector);
    is(filename.getAttribute("value"), expected[i].filename,
       "filename matches for " + cssSelector);

    is(attributes.length, expected[i].attributes.length,
       "we have the correct number of attributes");

    for (let j = 0; j < expected[i].attributes.length; j++) {
      is(attributes[j].getAttribute("value"), expected[i].attributes[j],
         "attribute[" + j + "] matches for " + cssSelector);
    }

    EventUtils.synthesizeMouseAtCenter(header, {}, type.ownerGlobal);
    yield tooltip.once("event-tooltip-ready");

    let editor = tooltip.eventEditors.get(contentBox).editor;
    is(editor.getText(), expected[i].handler,
       "handler matches for " + cssSelector);
  }

  tooltip.hide();
}
