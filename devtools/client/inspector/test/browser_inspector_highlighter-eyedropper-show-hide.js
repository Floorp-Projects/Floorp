/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Test the basic structure of the eye-dropper highlighter.

const HIGHLIGHTER_TYPE = "EyeDropper";
const ID = "eye-dropper-";

add_task(function* () {
  let helper = yield openInspectorForURL("data:text/html;charset=utf-8,eye-dropper test")
               .then(getHighlighterHelperFor(HIGHLIGHTER_TYPE));
  helper.prefix = ID;

  yield isInitiallyHidden(helper);
  yield canBeShownAndHidden(helper);

  helper.finalize();
});

function* isInitiallyHidden({isElementHidden}) {
  info("Checking that the eyedropper is hidden by default");

  let hidden = yield isElementHidden("root");
  ok(hidden, "The eyedropper is hidden by default");
}

function* canBeShownAndHidden({show, hide, isElementHidden, getElementAttribute}) {
  info("Asking to show and hide the highlighter actually works");

  yield show("html");
  let hidden = yield isElementHidden("root");
  ok(!hidden, "The eyedropper is now shown");

  let style = yield getElementAttribute("root", "style");
  is(style, "top:100px;left:100px;", "The eyedropper is correctly positioned");

  yield hide();
  hidden = yield isElementHidden("root");
  ok(hidden, "The eyedropper is now hidden again");
}
