/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test confirms that XUL attributes don't show up as empty
// attributes after being deleted

const TEST_URL = URL_ROOT + "doc_markup_xul.xul";

add_task(function* () {
  let {inspector, testActor} = yield openInspectorForURL(TEST_URL);

  let panelFront = yield getNodeFront("#test", inspector);
  ok(panelFront.hasAttribute("id"),
     "panelFront has id attribute in the beginning");

  info("Removing panel's id attribute");
  let onMutation = inspector.once("markupmutation");
  yield testActor.removeAttribute("#test", "id");

  info("Waiting for markupmutation");
  yield onMutation;

  is(panelFront.hasAttribute("id"), false,
     "panelFront doesn't have id attribute anymore");
});
