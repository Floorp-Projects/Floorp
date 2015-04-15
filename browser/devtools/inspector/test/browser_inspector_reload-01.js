/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// A test to ensure reloading a page doesn't break the inspector.

// Reload should reselect the currently selected markup view element.
// This should work even when an element whose selector needs escaping
// is selected (bug 1002280).
const TEST_URI = "data:text/html,<p id='1'>p</p>";

add_task(function* () {
  let { inspector, toolbox } = yield openInspectorForURL(TEST_URI);
  yield selectNode("p", inspector);

  let markupLoaded = inspector.once("markuploaded");

  info("Reloading page.");
  content.location.reload();

  info("Waiting for markupview to load after reload.");
  yield markupLoaded;

  let nodeFront = yield getNodeFront("p", inspector);
  is(inspector.selection.nodeFront, nodeFront, "<p> selected after reload.");

  info("Selecting a node to see that inspector still works.");
  yield selectNode("body", inspector);
});
