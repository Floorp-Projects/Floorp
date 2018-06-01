/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Test that the eyedropper opens correctly even when the page defines CSP headers.

const HIGHLIGHTER_TYPE = "EyeDropper";
const ID = "eye-dropper-";
const TEST_URI = URL_ROOT + "doc_inspector_csp.html";

add_task(async function() {
  const helper = await openInspectorForURL(TEST_URI)
               .then(getHighlighterHelperFor(HIGHLIGHTER_TYPE));
  helper.prefix = ID;
  const {show, hide, finalize, isElementHidden, waitForElementAttributeSet} = helper;

  info("Try to display the eyedropper");
  await show("html");

  const hidden = await isElementHidden("root");
  ok(!hidden, "The eyedropper is now shown");

  info("Wait until the eyedropper is done taking a screenshot of the page");
  await waitForElementAttributeSet("root", "drawn", helper);
  ok(true, "The image data was retrieved successfully from the window");

  await hide();
  finalize();
});
