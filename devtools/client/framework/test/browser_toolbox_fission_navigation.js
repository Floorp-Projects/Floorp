/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const EXAMPLE_COM_URI =
  "https://example.com/document-builder.sjs?html=<div id=com>com";
const EXAMPLE_ORG_URI =
  "https://example.org/document-builder.sjs?html=<div id=org>org";

add_task(async function () {
  const tab = await addTab(EXAMPLE_COM_URI);

  const toolbox = await openToolboxForTab(tab, "inspector");
  const comNode = await getNodeBySelector(toolbox, "#com");
  ok(comNode, "Found node for the COM page");

  info("Navigate to the ORG page");
  await navigateTo(EXAMPLE_ORG_URI);
  const orgNode = await getNodeBySelector(toolbox, "#org");
  ok(orgNode, "Found node for the ORG page");

  info("Reload the ORG page");
  await navigateTo(EXAMPLE_ORG_URI);
  const orgNodeAfterReload = await getNodeBySelector(toolbox, "#org");
  ok(orgNodeAfterReload, "Found node for the ORG page after reload");
  isnot(orgNode, orgNodeAfterReload, "The new node is different");

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
});

async function getNodeBySelector(toolbox, selector) {
  const inspector = await toolbox.selectTool("inspector");
  return inspector.walker.querySelector(inspector.walker.rootNode, selector);
}
