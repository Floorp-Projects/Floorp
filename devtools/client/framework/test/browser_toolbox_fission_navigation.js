/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const EXAMPLE_COM_URI =
  "http://example.com/document-builder.sjs?html=<div id=com>com";
const EXAMPLE_NET_URI =
  "http://example.net/document-builder.sjs?html=<div id=net>net";

add_task(async function() {
  // Disable bfcache for Fission for now.
  // If Fission is disabled, the pref is no-op.
  await SpecialPowers.pushPrefEnv({
    set: [["fission.bfcacheInParent", false]],
  });

  info("Test with server side target switching ENABLED");
  await pushPref("devtools.target-switching.server.enabled", true);
  await testCase();

  info("Test with server side target switching DISABLED");
  await pushPref("devtools.target-switching.server.enabled", false);
  await testCase();
});

async function testCase() {
  const tab = await addTab(EXAMPLE_COM_URI);

  const toolbox = await openToolboxForTab(tab, "inspector");
  const comNode = await getNodeBySelector(toolbox, "#com");
  ok(comNode, "Found node for the COM page");

  info("Navigate to the NET page");
  await navigateTo(EXAMPLE_NET_URI);
  const netNode = await getNodeBySelector(toolbox, "#net");
  ok(netNode, "Found node for the NET page");

  info("Reload the NET page");
  await navigateTo(EXAMPLE_NET_URI);
  const netNodeAfterReload = await getNodeBySelector(toolbox, "#net");
  ok(netNodeAfterReload, "Found node for the NET page after reload");
  isnot(netNode, netNodeAfterReload, "The new node is different");

  info("Navigate back to the COM page");
  await navigateTo(EXAMPLE_COM_URI);
  const comNodeAfterNavigation = await getNodeBySelector(toolbox, "#com");
  ok(comNodeAfterNavigation, "Found node for the COM page after navigation");

  info("Navigate to about:blank");
  await navigateTo("about:blank");
  const blankBodyAfterNavigation = await getNodeBySelector(toolbox, "body");
  ok(
    blankBodyAfterNavigation,
    "Found node for the about:blank page after navigation"
  );

  info("Navigate to about:robots");
  await navigateTo("about:robots");
  const aboutRobotsAfterNavigation = await getNodeBySelector(
    toolbox,
    "div.container"
  );
  ok(
    aboutRobotsAfterNavigation,
    "Found node for the about:robots page after navigation"
  );
}

async function getNodeBySelector(toolbox, selector) {
  const inspector = await toolbox.selectTool("inspector");
  return inspector.walker.querySelector(inspector.walker.rootNode, selector);
}
