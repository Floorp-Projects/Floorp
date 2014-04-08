/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the checkbox to include browser styles works properly.

let test = asyncTest(function*() {
  yield addTab("data:text/html,default styles test");

  info("Creating the test document");
  content.document.body.innerHTML = '<style type="text/css"> ' +
    '.matches {color: #F00;}</style>' +
    '<span id="matches" class="matches">Some styled text</span>' +
    '</div>';
  content.document.title = "Style Inspector Default Styles Test";

  info("Opening the computed view");
  let {toolbox, inspector, view} = yield openComputedView();

  info("Selecting the test node");
  yield selectNode("#matches", inspector);

  info("Checking the default styles");
  is(isPropertyVisible("color", view), true,
    "span #matches color property is visible");
  is(isPropertyVisible("background-color", view), false,
    "span #matches background-color property is hidden");

  info("Toggling the browser styles");
  let doc = view.styleDocument;
  let checkbox = doc.querySelector(".includebrowserstyles");
  let onRefreshed = inspector.once("computed-view-refreshed");
  checkbox.click();
  yield onRefreshed;

  info("Checking the browser styles");
  is(isPropertyVisible("color", view), true,
    "span color property is visible");
  is(isPropertyVisible("background-color", view), true,
    "span background-color property is visible");
});

function isPropertyVisible(name, view) {
  info("Checking property visibility for " + name);
  let propertyViews = view.propertyViews;
  for each (let propView in propertyViews) {
    if (propView.name == name) {
      return propView.visible;
    }
  }
  return false;
}
