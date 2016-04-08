/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test shadow DOM content in the markupview.
// Note that many features are not yet enabled, but basic listing
// of elements should be working.
const TEST_URL = URL_ROOT + "doc_markup_anonymous.html";

add_task(function* () {
  Services.prefs.setBoolPref("dom.webcomponents.enabled", true);

  let {inspector} = yield openInspectorForURL(TEST_URL);

  let shadow = yield getNodeFront("#shadow", inspector.markup);
  let children = yield inspector.walker.children(shadow);

  is(shadow.numChildren, 3, "Children of the shadow root are counted");
  is(children.nodes.length, 3, "Children returned from walker");

  info("Checking the ::before pseudo element");
  let before = children.nodes[0];
  yield isEditingMenuDisabled(before, inspector);

  info("Checking the <h3> shadow element");
  let shadowChild1 = children.nodes[1];
  yield isEditingMenuDisabled(shadowChild1, inspector);

  info("Checking the <select> shadow element");
  let shadowChild2 = children.nodes[2];
  yield isEditingMenuDisabled(shadowChild2, inspector);
});
