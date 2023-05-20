/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URL = `data:text/html;charset=utf-8,
  <video id="with-children" controls>
    <div>some content</div>
  </video>
  <video id="no-children" controls></video>`;

add_task(async function () {
  info("Test a <video> element with no children, showAllAnonymousContent=true");
  const { inspector, markup } = await setup({ showAllAnonymousContent: true });

  info("Find the #no-children element.");
  const hostFront = await getNodeFront("#no-children", inspector);
  const hostContainer = markup.getContainer(hostFront);
  ok(
    hostContainer.canExpand,
    "<video controls/> has children in the markup view"
  );

  info("Expand the <video> element");
  await expandContainer(inspector, hostContainer);
  is(hostContainer.getChildContainers().length, 3, "video has 3 children");

  const shadowRootContainer = hostContainer.getChildContainers()[0];
  assertContainerHasText(shadowRootContainer, "#shadow-root");
});

add_task(async function () {
  info("Test a <video> element with children, showAllAnonymousContent=true");
  const { inspector, markup } = await setup({ showAllAnonymousContent: true });

  info("Find the #with-children element.");
  const hostFront = await getNodeFront("#with-children", inspector);
  const hostContainer = markup.getContainer(hostFront);
  ok(
    hostContainer.canExpand,
    "<video controls/> has children in the markup view"
  );

  info("Expand the <video> element");
  await expandContainer(inspector, hostContainer);
  is(hostContainer.getChildContainers().length, 4, "video has 4 children");

  const shadowRootContainer = hostContainer.getChildContainers()[0];
  assertContainerHasText(shadowRootContainer, "#shadow-root");

  const divContainer = hostContainer.getChildContainers()[1];
  assertContainerHasText(divContainer, "some content");
});

add_task(async function () {
  info(
    "Test a <video> element with no children, showAllAnonymousContent=false"
  );
  const { inspector, markup } = await setup({
    showAllAnonymousContent: false,
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

add_task(async function () {
  info("Test a <video> element with children, showAllAnonymousContent=false");
  const { inspector, markup } = await setup({
    showAllAnonymousContent: false,
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

async function setup({ showAllAnonymousContent }) {
  await pushPref(
    "devtools.inspector.showAllAnonymousContent",
    showAllAnonymousContent
  );

  const { inspector } = await openInspectorForURL(TEST_URL);
  const { markup } = inspector;
  return { inspector, markup };
}
