/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests adding, disble/enable, and removal of dom mutation breakpoints

"use strict";

// Import helpers for the inspector
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/inspector/test/shared-head.js",
  this
);

const DMB_TEST_URL =
  "https://example.com/browser/devtools/client/debugger/test/mochitest/examples/doc-dom-mutation.html";

async function enableMutationBreakpoints() {
  await pushPref("devtools.debugger.dom-mutation-breakpoints-visible", true);
}

add_task(async function () {
  // Enable features
  await enableMutationBreakpoints();
  await pushPref("devtools.debugger.map-scopes-enabled", true);

  info("Switches over to the inspector pane");

  const { inspector, toolbox } = await openInspectorForURL(DMB_TEST_URL);

  {
    info("Selecting the body node");
    await selectNode("body", inspector);

    info("Adding DOM mutation breakpoints to body");
    const allMenuItems = openContextMenuAndGetAllItems(inspector);

    const attributeMenuItem = allMenuItems.find(
      item => item.id === "node-menu-mutation-breakpoint-attribute"
    );
    attributeMenuItem.click();

    const subtreeMenuItem = allMenuItems.find(
      item => item.id === "node-menu-mutation-breakpoint-subtree"
    );
    subtreeMenuItem.click();
  }

  {
    info("Find and expand the shadow host.");
    const hostFront = await getNodeFront("#host", inspector);
    const hostContainer = inspector.markup.getContainer(hostFront);
    await expandContainer(inspector, hostContainer);

    info("Expand the shadow root");
    const shadowRootContainer = hostContainer.getChildContainers()[0];
    await expandContainer(inspector, shadowRootContainer);

    info("Select the div under the shadow root");
    const divContainer = shadowRootContainer.getChildContainers()[0];
    await selectNode(divContainer.node, inspector);

    const allMenuItems = openContextMenuAndGetAllItems(inspector);
    info("Adding attribute breakpoint.");
    const attributeMenuItem = allMenuItems.find(
      item => item.id === "node-menu-mutation-breakpoint-attribute"
    );
    attributeMenuItem.click();
  }

  info("Switches over to the debugger pane");
  await toolbox.selectTool("jsdebugger");

  const dbg = createDebuggerContext(toolbox);

  info("Confirms that one DOM mutation breakpoint exists");
  const mutationItem = await waitForElement(dbg, "domMutationItem");
  ok(mutationItem, "A DOM mutation breakpoint exists");

  mutationItem.scrollIntoView();

  info("Enabling and disabling the DOM mutation breakpoint works");
  const checkbox = mutationItem.querySelector("input");
  checkbox.click();
  await waitFor(() => !checkbox.checked);
  checkbox.click();
  await waitFor(() => checkbox.checked);

  info("Changing attribute to trigger debugger pause");
  SpecialPowers.spawn(gBrowser.selectedBrowser, [], function () {
    content.document.querySelector("#attribute").click();
  });
  await waitForPaused(dbg);
  let whyPaused = await waitFor(
    () => dbg.win.document.querySelector(".why-paused")?.innerText
  );
  is(
    whyPaused,
    `Paused on DOM mutation\nDOM Mutation: 'attributeModified'\nbody`
  );

  await resume(dbg);

  info("Changing style to trigger debugger pause");
  SpecialPowers.spawn(gBrowser.selectedBrowser, [], function () {
    content.document.querySelector("#style-attribute").click();
  });
  await waitForPaused(dbg);
  await resume(dbg);

  info("Changing attribute in shadow dom to trigger debugger pause");
  SpecialPowers.spawn(gBrowser.selectedBrowser, [], function () {
    content.document.querySelector("#shadow-attribute").click();
  });
  await waitForPaused(dbg);
  await resume(dbg);

  info("Adding element in subtree to trigger debugger pause");
  SpecialPowers.spawn(gBrowser.selectedBrowser, [], function () {
    content.document.querySelector("#add-in-subtree").click();
  });
  await waitForPaused(dbg);
  whyPaused = await waitFor(
    () => dbg.win.document.querySelector(".why-paused")?.innerText
  );
  is(
    whyPaused,
    `Paused on DOM mutation\nDOM Mutation: 'subtreeModified'\nbodyAdded:div#dynamic`
  );

  await resume(dbg);

  info("Removing element in subtree to trigger debugger pause");
  SpecialPowers.spawn(gBrowser.selectedBrowser, [], function () {
    content.document.querySelector("#remove-in-subtree").click();
  });
  await waitForPaused(dbg);
  whyPaused = await waitFor(
    () => dbg.win.document.querySelector(".why-paused")?.innerText
  );
  is(
    whyPaused,
    `Paused on DOM mutation\nDOM Mutation: 'subtreeModified'\nbodyRemoved:div#dynamic`
  );

  await resume(dbg);

  info("Blackboxing the source prevents debugger pause");
  await waitForSource(dbg, "dom-mutation.original.js");

  const source = findSource(dbg, "dom-mutation.original.js");

  await selectSource(dbg, source);
  await clickElement(dbg, "blackbox");
  await waitForDispatch(dbg.store, "BLACKBOX_WHOLE_SOURCES");

  SpecialPowers.spawn(gBrowser.selectedBrowser, [], function () {
    content.document.querySelector("#blackbox").click();
  });

  await waitForPaused(dbg, "click.js");
  await resume(dbg);

  await selectSource(dbg, source);
  await clickElement(dbg, "blackbox");
  await waitForDispatch(dbg.store, "UNBLACKBOX_WHOLE_SOURCES");

  info("Removing breakpoints works");
  dbg.win.document.querySelector(".dom-mutation-list .close-btn").click();
  await waitForAllElements(dbg, "domMutationItem", 2, true);
});
