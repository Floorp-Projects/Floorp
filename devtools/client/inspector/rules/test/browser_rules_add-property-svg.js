/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests editing SVG styles using the rules view.

var TEST_URL = "chrome://global/skin/icons/warning.svg";
var TEST_SELECTOR = "path";

add_task(function* () {
  yield addTab(TEST_URL);
  let {inspector, view} = yield openRuleView();
  yield selectNode(TEST_SELECTOR, inspector);

  info("Test creating a new property");
  yield addProperty(view, 0, "fill", "red");

  is((yield getComputedStyleProperty(TEST_SELECTOR, null, "fill")),
     "rgb(255, 0, 0)", "The fill was changed to red");
});
