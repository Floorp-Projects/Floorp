/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* globals getTestActorWithoutToolbox */
"use strict";

// Tests for inspect node in browser context menu

const FRAME_URI = "data:text/html;charset=utf-8," +
  encodeURI(`<div id="in-frame">div in the iframe</div>`);
const HTML = `
  <div id="salutation">Salution in top document</div>
  <iframe src="${FRAME_URI}"></iframe>
`;

const TEST_URI = "data:text/html;charset=utf-8," + encodeURI(HTML);

add_task(function* () {
  let tab = yield addTab(TEST_URI);
  let testActor = yield getTestActorWithoutToolbox(tab);

  yield testContextMenuWithinIframe(testActor);
});

function* testContextMenuWithinIframe(testActor) {
  info("Opening inspector via 'Inspect Element' context menu item within an iframe");
  let selector = ["iframe", "#in-frame"];
  yield clickOnInspectMenuItem(testActor, selector);

  info("Checking inspector state.");
  let inspector = getActiveInspector();
  let nodeFront = yield getNodeFrontInFrame("#in-frame", "iframe", inspector);

  is(inspector.selection.nodeFront, nodeFront,
     "Right node is selected in the markup view");
}
