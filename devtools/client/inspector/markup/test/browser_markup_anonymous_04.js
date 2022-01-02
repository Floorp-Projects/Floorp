/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test native anonymous content in the markupview with
// devtools.inspector.showAllAnonymousContent set to true
const TEST_URL = URL_ROOT + "doc_markup_anonymous.html";
const PREF = "devtools.inspector.showAllAnonymousContent";

add_task(async function() {
  Services.prefs.setBoolPref(PREF, true);

  const { inspector } = await openInspectorForURL(TEST_URL);

  const native = await getNodeFront("#native", inspector);

  // Markup looks like: <div><input type="file"></div>
  const nativeChildren = await inspector.walker.children(native);
  is(nativeChildren.nodes.length, 1, "Children returned from walker");

  info("Checking the input element");
  const child = nativeChildren.nodes[0];
  ok(!child.isAnonymous, "<input type=file> is not anonymous");

  const grandchildren = await inspector.walker.children(child);
  is(
    grandchildren.nodes.length,
    2,
    "<input type=file> has native anonymous children"
  );

  for (const node of grandchildren.nodes) {
    ok(node.isAnonymous, "Child is anonymous");
    ok(node._form.isNativeAnonymous, "Child is native anonymous");
    await isEditingMenuDisabled(node, inspector);
  }
});
