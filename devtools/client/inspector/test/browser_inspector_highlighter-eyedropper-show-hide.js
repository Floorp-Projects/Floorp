/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Test the basic structure of the eye-dropper highlighter.

const HIGHLIGHTER_TYPE = "EyeDropper";
const ID = "eye-dropper-";

add_task(async function() {
  const helper =
    await openInspectorForURL("data:text/html;charset=utf-8,eye-dropper test")
      .then(getHighlighterHelperFor(HIGHLIGHTER_TYPE));
  helper.prefix = ID;

  await isInitiallyHidden(helper);
  await canBeShownAndHidden(helper);

  helper.finalize();
});

async function isInitiallyHidden({isElementHidden}) {
  info("Checking that the eyedropper is hidden by default");

  const hidden = await isElementHidden("root");
  ok(hidden, "The eyedropper is hidden by default");
}

async function canBeShownAndHidden({show, hide, isElementHidden, getElementAttribute}) {
  info("Asking to show and hide the highlighter actually works");

  await show("html");
  let hidden = await isElementHidden("root");
  ok(!hidden, "The eyedropper is now shown");

  const style = await getElementAttribute("root", "style");
  is(style, "top:100px;left:100px;", "The eyedropper is correctly positioned");

  await hide();
  hidden = await isElementHidden("root");
  ok(hidden, "The eyedropper is now hidden again");
}
