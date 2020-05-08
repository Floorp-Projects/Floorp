/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test target-switching with the inspector.
// This test should check both fission and non-fission target switching.

const PARENT_PROCESS_URI = "about:robots";
const EXAMPLE_COM_URI =
  "http://example.com/document-builder.sjs?html=<div id=com>com";
const EXAMPLE_NET_URI =
  "http://example.net/document-builder.sjs?html=<div id=net>net";

add_task(async function() {
  await pushPref("devtools.target-switching.enabled", true);

  const { inspector } = await openInspectorForURL(PARENT_PROCESS_URI);
  const aboutRobotsNodeFront = await getNodeFront(".title-text", inspector);
  ok(!!aboutRobotsNodeFront, "Can retrieve a node front from about:robots");

  info("Navigate to " + EXAMPLE_COM_URI);
  await navigateTo(EXAMPLE_COM_URI);
  const comNodeFront = await getNodeFront("#com", inspector);
  ok(!!comNodeFront, "Can retrieve a node front from example.com");

  await navigateTo(EXAMPLE_NET_URI);
  const netNodeFront = await getNodeFront("#net", inspector);
  ok(!!netNodeFront, "Can retrieve a node front from example.net");

  await navigateTo(PARENT_PROCESS_URI);
  const aboutRobotsNodeFront2 = await getNodeFront(".title-text", inspector);
  ok(!!aboutRobotsNodeFront2, "Can retrieve a node front from about:robots");
});
