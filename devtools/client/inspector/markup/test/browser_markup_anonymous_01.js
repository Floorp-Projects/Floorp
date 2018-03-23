/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test native anonymous content in the markupview.
const TEST_URL = URL_ROOT + "doc_markup_anonymous.html";

add_task(async function() {
  let {inspector} = await openInspectorForURL(TEST_URL);

  let pseudo = await getNodeFront("#pseudo", inspector);

  // Markup looks like: <div><::before /><span /><::after /></div>
  let children = await inspector.walker.children(pseudo);
  is(children.nodes.length, 3, "Children returned from walker");

  info("Checking the ::before pseudo element");
  let before = children.nodes[0];
  await isEditingMenuDisabled(before, inspector);

  info("Checking the normal child element");
  let span = children.nodes[1];
  await isEditingMenuEnabled(span, inspector);

  info("Checking the ::after pseudo element");
  let after = children.nodes[2];
  await isEditingMenuDisabled(after, inspector);

  let native = await getNodeFront("#native", inspector);

  // Markup looks like: <div><video controls /></div>
  let nativeChildren = await inspector.walker.children(native);
  is(nativeChildren.nodes.length, 1, "Children returned from walker");

  info("Checking the video element");
  let video = nativeChildren.nodes[0];
  ok(!video.isAnonymous, "<video> is not anonymous");

  let videoChildren = await inspector.walker.children(video);
  is(videoChildren.nodes.length, 0,
    "No native children returned from walker for <video> by default");
});
