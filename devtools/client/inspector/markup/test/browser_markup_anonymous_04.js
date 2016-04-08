/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test native anonymous content in the markupview with
// devtools.inspector.showAllAnonymousContent set to true
const TEST_URL = URL_ROOT + "doc_markup_anonymous.html";
const PREF = "devtools.inspector.showAllAnonymousContent";

add_task(function* () {
  Services.prefs.setBoolPref(PREF, true);

  let {inspector} = yield openInspectorForURL(TEST_URL);

  let native = yield getNodeFront("#native", inspector);

  // Markup looks like: <div><video controls /></div>
  let nativeChildren = yield inspector.walker.children(native);
  is(nativeChildren.nodes.length, 1, "Children returned from walker");

  info("Checking the video element");
  let video = nativeChildren.nodes[0];
  ok(!video.isAnonymous, "<video> is not anonymous");

  let videoChildren = yield inspector.walker.children(video);
  is(videoChildren.nodes.length, 3, "<video> has native anonymous children");

  for (let node of videoChildren.nodes) {
    ok(node.isAnonymous, "Child is anonymous");
    ok(!node._form.isXBLAnonymous, "Child is not XBL anonymous");
    ok(!node._form.isShadowAnonymous, "Child is not shadow anonymous");
    ok(node._form.isNativeAnonymous, "Child is native anonymous");
    yield isEditingMenuDisabled(node, inspector);
  }
});
