/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Test that the eyedropper opens correctly even when the page defines CSP headers.

const HIGHLIGHTER_TYPE = "EyeDropper";
const ID = "eye-dropper-";
const TEST_URI = URL_ROOT + "doc_inspector_csp.html";

add_task(function* () {
  let helper = yield openInspectorForURL(TEST_URI)
               .then(getHighlighterHelperFor(HIGHLIGHTER_TYPE));
  helper.prefix = ID;
  let {show, hide, finalize, isElementHidden, waitForElementAttributeSet} = helper;

  info("Try to display the eyedropper");
  yield show("html");

  let hidden = yield isElementHidden("root");
  ok(!hidden, "The eyedropper is now shown");

  info("Wait until the eyedropper is done taking a screenshot of the page");
  yield waitForElementAttributeSet("root", "drawn", helper);
  ok(true, "The image data was retrieved successfully from the window");

  yield hide();
  finalize();
});
