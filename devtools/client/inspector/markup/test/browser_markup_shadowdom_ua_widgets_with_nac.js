/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URL = `data:text/html;charset=utf-8,` + encodeURIComponent(`
  <video id="video-with-subtitles" style="border: 4px solid red" controls>
    <track label="en" kind="subtitles" srclang="en" src="data:text/vtt,WEBVTT" default>
  </video>`);

const TEST_ID = "#video-with-subtitles";

// Test that Inspector can show Native Anonymous Content (nac) in user agent widgets, as
// siblings of the ua widget closed shadow-root.

add_task(async function testWithoutShowAllAnonymousContent() {
  info("Test a <video> element with subtitles, without showAllAnonymousContent");
  const { inspector } = await setup({ showAllAnonymousContent: false });

  // We should only see the shadow root and the authored <track> element.
  const tree = `
  video
    #shadow-root!ignore-children
    track`;
  await assertMarkupViewAsTree(tree, TEST_ID, inspector);
});

add_task(async function testWithShowAllAnonymousContent() {
  info("Test a <video> element with subtitles, expect to see native anonymous content");
  const { inspector } = await setup({ showAllAnonymousContent: true });

  // We should only see the shadow root, the <track> and two NAC elements <img> and <div>.
  const tree = `
  video
    #shadow-root!ignore-children
    track
    img
    class="caption-box"`;
  await assertMarkupViewAsTree(tree, TEST_ID, inspector);
});

async function setup({ showAllAnonymousContent }) {
  await pushPref("dom.ua_widget.enabled", true);
  await pushPref("devtools.inspector.showUserAgentShadowRoots", true);
  await pushPref("devtools.inspector.showAllAnonymousContent", showAllAnonymousContent);

  const { inspector } = await openInspectorForURL(TEST_URL);
  const { markup } = inspector;
  return { inspector, markup };
}
