/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Test that highlighted nodes can be scrolled.
// TODO: This doesn't test anything useful. See b.m.o 1035661.
const IFRAME_SRC = "data:text/html;charset=utf-8," +
  "<div style='height:500px; width:500px; border:1px solid gray;'>" +
    "big div" +
  "</div>";

const TEST_URI = "data:text/html;charset=utf-8," +
  "<p>browser_inspector_scrolling.js</p>" +
  "<iframe src=\"" + IFRAME_SRC + "\" />";

let test = asyncTest(function* () {
  let { inspector, toolbox } = yield openInspectorForURL(TEST_URI);

  let iframe = getNode("iframe");
  let div = getNode("div", { document: iframe.contentDocument });

  info("Waiting for highlighter box model to appear.");
  yield toolbox.highlighter.showBoxModel(getNodeFront(div));

  let scrolled = once(gBrowser.selectedBrowser, "scroll");

  info("Scrolling iframe.");
  EventUtils.synthesizeWheel(div, 10, 10,
    { deltaY: 50.0, deltaMode: WheelEvent.DOM_DELTA_PIXEL },
    iframe.contentWindow);

  info("Waiting for scroll event");
  yield scrolled;

  let isRetina = devicePixelRatio === 2;
  is(iframe.contentDocument.body.scrollTop,
    isRetina ? 25 : 50, "inspected iframe scrolled");

  info("Hiding box model.");
  yield toolbox.highlighter.hideBoxModel();
});
