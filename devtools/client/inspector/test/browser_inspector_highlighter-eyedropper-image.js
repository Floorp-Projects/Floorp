/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the eyedropper icon in the toolbar is enabled when viewing an image.

const TEST_URL = URL_ROOT + "img_browser_inspector_highlighter-eyedropper-image.png";

add_task(async function() {
  const {inspector} = await openInspectorForURL(TEST_URL);
  info("Check the inspector toolbar when viewing an image");
  const button = inspector.panelDoc.querySelector("#inspector-eyedropper-toggle");
  ok(!button.disabled, "The button is enabled in the toolbar");
});
