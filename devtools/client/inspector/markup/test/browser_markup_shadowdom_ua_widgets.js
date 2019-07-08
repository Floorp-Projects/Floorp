/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URL = `data:text/html;charset=utf-8,
  <video id="with-children" controls>
    <div>some content</div>
  </video>
  <video id="no-children" controls></video>`;

add_task(async function() {
  info(
    "Test a <video> element with no children, showUserAgentShadowRoots=true"
  );
  const { inspector, markup } = await setup({ showUserAgentShadowRoots: true });

  info("Find the #no-children element.");
  const hostFront = await getNodeFront("#no-children", inspector);
  const hostContainer = markup.getContainer(hostFront);
  ok(
    hostContainer.canExpand,
    "<video controls/> has children in the markup view"
  );

  info("Expand the <video> element");
  await expandContainer(inspector, hostContainer);
  is(hostContainer.getChildContainers().length, 1, "video has 1 child");

  const shadowRootContainer = hostContainer.getChildContainers()[0];
  assertContainerHasText(shadowRootContainer, "#shadow-root");
});

add_task(async function() {
  info("Test a <video> element with children, showUserAgentShadowRoots=true");
  const { inspector, markup } = await setup({ showUserAgentShadowRoots: true });

  info("Find the #with-children element.");
  const hostFront = await getNodeFront("#with-children", inspector);
  const hostContainer = markup.getContainer(hostFront);
  ok(
    hostContainer.canExpand,
    "<video controls/> has children in the markup view"
  );

  info("Expand the <video> element");
  await expandContainer(inspector, hostContainer);
  is(hostContainer.getChildContainers().length, 2, "video has 2 children");

  const shadowRootContainer = hostContainer.getChildContainers()[0];
  assertContainerHasText(shadowRootContainer, "#shadow-root");

  const divContainer = hostContainer.getChildContainers()[1];
  assertContainerHasText(divContainer, "some content");
});

add_task(async function() {
  info(
    "Test a <video> element with no children, showUserAgentShadowRoots=false"
  );
  const { inspector, markup } = await setup({
    showUserAgentShadowRoots: false,
  });

  info("Find the #no-children element.");
  const hostFront = await getNodeFront("#no-children", inspector);
  const hostContainer = markup.getContainer(hostFront);
  ok(!hostContainer.getChildContainers(), "video has no children");
  ok(
    !hostContainer.canExpand,
    "<video controls/> shows no children in the markup view"
  );
});

add_task(async function() {
  info("Test a <video> element with children, showUserAgentShadowRoots=false");
  const { inspector, markup } = await setup({
    showUserAgentShadowRoots: false,
  });

  info("Find the #with-children element.");
  const hostFront = await getNodeFront("#with-children", inspector);
  const hostContainer = markup.getContainer(hostFront);
  ok(
    hostContainer.canExpand,
    "<video controls/> has children in the markup view"
  );

  info("Expand the <video> element");
  await expandContainer(inspector, hostContainer);
  is(hostContainer.getChildContainers().length, 1, "video has 1 child");

  const divContainer = hostContainer.getChildContainers()[0];
  assertContainerHasText(divContainer, "some content");
});

async function setup({ showUserAgentShadowRoots }) {
  await pushPref(
    "devtools.inspector.showUserAgentShadowRoots",
    showUserAgentShadowRoots
  );

  const { inspector } = await openInspectorForURL(TEST_URL);
  const { markup } = inspector;
  return { inspector, markup };
}
