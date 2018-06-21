/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

// Test XBL anonymous content in the markupview
const TEST_URL = "chrome://devtools/content/scratchpad/index.xul";

add_task(async function() {
  const {inspector} = await openInspectorForURL(TEST_URL);

  const toolbarbutton = await getNodeFront("toolbarbutton", inspector);
  const children = await inspector.walker.children(toolbarbutton);

  is(toolbarbutton.numChildren, 3, "Correct number of children");
  is(children.nodes.length, 3, "Children returned from walker");

  is(toolbarbutton.isAnonymous, false, "Toolbarbutton is not anonymous");
  await isEditingMenuEnabled(toolbarbutton, inspector);

  for (const node of children.nodes) {
    ok(node.isAnonymous, "Child is anonymous");
    ok(node._form.isXBLAnonymous, "Child is XBL anonymous");
    ok(!node._form.isShadowAnonymous, "Child is not shadow anonymous");
    ok(!node._form.isNativeAnonymous, "Child is not native anonymous");
    await isEditingMenuDisabled(node, inspector);
  }
});
