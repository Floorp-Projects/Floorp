/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test confirms that XUL attributes don't show up as empty
// attributes after being deleted

const TEST_URL = TEST_URL_ROOT + "doc_markup_xul.xul";

add_task(function*() {
  let {inspector} = yield addTab(TEST_URL).then(openInspector);

  let panel = yield getNode("#test", inspector);
  let panelFront = yield getNodeFront("#test", inspector);

  ok(panelFront.hasAttribute("id"), "panelFront has id attribute in the beginning");

  info("Removing panel's id attribute");
  panel.removeAttribute("id");

  info("Waiting for markupmutation");
  yield inspector.once("markupmutation");

  is(panelFront.hasAttribute("id"), false, "panelFront doesn't have id attribute anymore");
});
