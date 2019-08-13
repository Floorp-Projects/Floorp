/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */


// Tests adding, disble/enable, and removal of dom mutation breakpoints


/* import-globals-from ../../../inspector/test/shared-head.js */

// Import helpers for the inspector
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/inspector/test/shared-head.js",
  this
);

const DMB_TEST_URL = "http://example.com/browser/devtools/client/debugger/test/mochitest/examples/doc-dom-mutation.html";

add_task(async function() {
  // Enable features
  await pushPref("devtools.debugger.features.dom-mutation-breakpoints", true);
  await pushPref("devtools.markup.mutationBreakpoints.enabled", true);
  await pushPref("devtools.debugger.dom-mutation-breakpoints-visible", true);

  info("Switches over to the inspector pane");

  const { inspector, toolbox } = await openInspectorForURL(DMB_TEST_URL);

  info("Sellecting the body node");
  await selectNode("body", inspector);

  info("Adding a DOM mutation breakpoint to body");
  const allMenuItems = openContextMenuAndGetAllItems(inspector);

  const breakOnMenuItem = allMenuItems.find(item => item.id === "node-menu-mutation-breakpoint-attribute");
  breakOnMenuItem.click();

  info("Switches over to the debugger pane");
  await toolbox.selectTool("jsdebugger");

  const dbg = createDebuggerContext(toolbox);

  info("Confirms that one DOM mutation breakpoint exists");
  const mutationItem = await waitForElementWithSelector(dbg, ".dom-mutation-list li");
  ok(mutationItem, "A DOM mutation breakpoint exists");

  mutationItem.scrollIntoView();

  info("Enabling and disabling the DOM mutation breakpoint works ");
  const checkbox = mutationItem.querySelector("input");
  checkbox.click();
  await waitFor(() => !checkbox.checked);
  checkbox.click();
  await waitFor(() => checkbox.checked);

  info("Changing attribute to trigger debugger pause");
  ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    content.document.querySelector("button").click();
  });
  await waitForPaused(dbg);
  await resume(dbg);
});