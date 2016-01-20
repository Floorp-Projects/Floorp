/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the computed view shows the original source link when source maps
// are enabled.

const TESTCASE_URI = URL_ROOT_SSL + "doc_sourcemaps.html";
const PREF = "devtools.styleeditor.source-maps-enabled";
const SCSS_LOC = "doc_sourcemaps.scss:4";
const CSS_LOC = "doc_sourcemaps.css:1";

add_task(function*() {
  info("Turning the pref " + PREF + " on");
  Services.prefs.setBoolPref(PREF, true);

  yield addTab(TESTCASE_URI);
  let {toolbox, inspector, view} = yield openComputedView();
  yield selectNode("div", inspector);

  info("Expanding the first property");
  yield expandComputedViewPropertyByIndex(view, 0);

  info("Verifying the link text");
  // Forcing a call to updateSourceLink on the SelectorView here. The
  // computed-view already does it, but we have no way of waiting for it to be
  // done here, so just call it again and wait for the returned promise to
  // resolve.
  let propertyView = getComputedViewPropertyView(view, "color");
  yield propertyView.matchedSelectorViews[0].updateSourceLink();
  verifyLinkText(view, SCSS_LOC);

  info("Toggling the pref");
  let onLinksUpdated = inspector.once("computed-view-sourcelinks-updated");
  Services.prefs.setBoolPref(PREF, false);
  yield onLinksUpdated;

  info("Verifying that the link text has changed after the pref change");
  yield verifyLinkText(view, CSS_LOC);

  info("Toggling the pref again");
  onLinksUpdated = inspector.once("computed-view-sourcelinks-updated");
  Services.prefs.setBoolPref(PREF, true);
  yield onLinksUpdated;

  info("Testing that clicking on the link works");
  yield testClickingLink(toolbox, view);

  info("Turning the pref " + PREF + " off");
  Services.prefs.clearUserPref(PREF);
});

function* testClickingLink(toolbox, view) {
  let onEditor = waitForStyleEditor(toolbox, "doc_sourcemaps.scss");

  info("Clicking the computedview stylesheet link");
  let link = getComputedViewLinkByIndex(view, 0);
  link.scrollIntoView();
  link.click();

  let editor = yield onEditor;

  let {line} = editor.sourceEditor.getCursor();
  is(line, 3, "cursor is at correct line number in original source");
}

function verifyLinkText(view, text) {
  let link = getComputedViewLinkByIndex(view, 0);
  is(link.textContent, text,
    "Linked text changed to display the correct location");
}
