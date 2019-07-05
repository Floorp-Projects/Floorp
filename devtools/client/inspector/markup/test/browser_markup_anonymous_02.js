/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

// Test XBL anonymous content in the markupview
const TEST_URL = URL_ROOT + "doc_markup_anonymous_xul.xul";

add_task(async function() {
  const { inspector } = await openInspectorForURL(TEST_URL);

  const boundNode = await getNodeFront("#xbl-host", inspector);
  const children = await inspector.walker.children(boundNode);

  is(boundNode.numChildren, 2, "Correct number of children");
  is(children.nodes.length, 2, "Children returned from walker");

  is(boundNode.isAnonymous, false, "Node with XBL binding is not anonymous");
  await isEditingMenuEnabled(boundNode, inspector);

  for (const node of children.nodes) {
    ok(node.isAnonymous, "Child is anonymous");
    ok(node._form.isXBLAnonymous, "Child is XBL anonymous");
    ok(!node._form.isShadowAnonymous, "Child is not shadow anonymous");
    ok(!node._form.isNativeAnonymous, "Child is not native anonymous");
    await isEditingMenuDisabled(node, inspector);
  }
});
