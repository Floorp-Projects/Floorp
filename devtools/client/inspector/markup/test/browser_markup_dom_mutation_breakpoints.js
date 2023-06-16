/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/debugger/test/mochitest/shared-head.js",
  this
);

function toggleMutationBreakpoint(inspector) {
  const allMenuItems = openContextMenuAndGetAllItems(inspector);
  const attributeMenuItem = allMenuItems.find(
    ({ id }) => id === "node-menu-mutation-breakpoint-attribute"
  );
  attributeMenuItem.click();
}

// Test inspector markup view handling DOM mutation breakpoints icons
// The icon should display when a breakpoint exists for a given node
add_task(async function () {
  const { inspector } = await openInspectorForURL(
    "data:text/html;charset=utf-8,<h1>foo</h1><span>bar</span>"
  );

  await selectNode("span", inspector);
  toggleMutationBreakpoint(inspector);

  const span = await getContainerForSelector("span", inspector);
  const mutationMarker = span.tagLine.querySelector(
    ".markup-tag-mutation-marker"
  );

  ok(
    mutationMarker.classList.contains("has-mutations"),
    "has-mutations class is present"
  );

  toggleMutationBreakpoint(inspector);
  await waitFor(() => !mutationMarker.classList.contains("has-mutations"));

  ok(true, "has-mutations class is not present");
});

// Test that the inspector markup view dom mutation breakpoint icon behaves
// correctly when disabled
add_task(async function () {
  await pushPref("devtools.debugger.dom-mutation-breakpoints-visible", true);
  const { inspector, toolbox } = await openInspectorForURL(
    "data:text/html;charset=utf-8,<h1>foo</h1><span>bar</span>"
  );

  await selectNode("span", inspector);
  toggleMutationBreakpoint(inspector);

  const span = await getContainerForSelector("span", inspector);
  const mutationMarker = span.tagLine.querySelector(
    ".markup-tag-mutation-marker"
  );

  ok(
    mutationMarker.classList.contains("has-mutations"),
    "has-mutations class is present"
  );
  is(
    mutationMarker.classList.contains("mutation-breakpoint-disabled"),
    false,
    "mutation-breakpoint-disabled class is not present"
  );

  info("Switch over to the debugger pane");
  await toolbox.selectTool("jsdebugger");

  const dbg = createDebuggerContext(toolbox);

  const mutationItem = await waitForElement(dbg, "domMutationItem");
  mutationItem.scrollIntoView();

  info("Disable the DOM mutation breakpoint");
  const checkbox = mutationItem.querySelector("input");
  checkbox.click();
  await waitFor(() => !checkbox.checked);

  await waitFor(
    () =>
      mutationMarker.classList.contains("has-mutations") &&
      mutationMarker.classList.contains("mutation-breakpoint-disabled")
  );

  ok(
    true,
    "has-mutations and mutation-breakpoint-disabled classes are both present"
  );

  info("Re-enable the DOM mutation breakpoint");
  checkbox.click();
  await waitFor(() => checkbox.checked);

  await waitFor(
    () =>
      mutationMarker.classList.contains("has-mutations") &&
      !mutationMarker.classList.contains("mutation-breakpoint-disabled")
  );

  ok(
    true,
    "has-mutation class is present, mutation-breakpoint-disabled is not present"
  );

  // Test re-enabling disabled dom mutation breakpoint from inspector
  info("Disable the DOM mutation breakpoint");
  checkbox.click();
  await waitFor(() => !checkbox.checked);

  await waitFor(
    () =>
      mutationMarker.classList.contains("has-mutations") &&
      mutationMarker.classList.contains("mutation-breakpoint-disabled")
  );

  ok(
    true,
    "has-mutations and mutation-breakpoint-disabled classes are both present"
  );

  info("Switch over to the inspector pane");
  await toolbox.selectTool("inspector");

  toggleMutationBreakpoint(inspector);
  await waitFor(
    () =>
      mutationMarker.classList.contains("has-mutations") &&
      !mutationMarker.classList.contains("mutation-breakpoint-disabled")
  );

  ok(
    true,
    "has-mutation class is present, mutation-breakpoint-disabled is not present"
  );
});

// Test icon behavior with multiple breakpoints on the same node.
add_task(async function () {
  await pushPref("devtools.debugger.dom-mutation-breakpoints-visible", true);
  const { inspector, toolbox } = await openInspectorForURL(
    "data:text/html;charset=utf-8,<h1>foo</h1><span>bar</span>"
  );

  await selectNode("span", inspector);
  const span = await getContainerForSelector("span", inspector);
  const mutationMarker = span.tagLine.querySelector(
    ".markup-tag-mutation-marker"
  );

  info("Add 2 DOM mutation breakpoints");
  const allMenuItems = openContextMenuAndGetAllItems(inspector);

  const attributeMenuItem = allMenuItems.find(
    item => item.id === "node-menu-mutation-breakpoint-attribute"
  );
  attributeMenuItem.click();

  const subtreeMenuItem = allMenuItems.find(
    item => item.id === "node-menu-mutation-breakpoint-subtree"
  );
  subtreeMenuItem.click();

  info("Switch over to the debugger pane");
  await toolbox.selectTool("jsdebugger");

  const dbg = createDebuggerContext(toolbox);

  info("Confirm that DOM mutation breakpoints exists");
  await waitForAllElements(dbg, "domMutationItem", 2, true);

  const mutationItem = await waitForElement(dbg, "domMutationItem");

  mutationItem.scrollIntoView();

  info("Disable 1 dom mutation breakpoint");
  const checkbox = mutationItem.querySelector("input");
  checkbox.click();
  await waitFor(() => !checkbox.checked);

  await waitFor(
    () =>
      mutationMarker.classList.contains("has-mutations") &&
      !mutationMarker.classList.contains("mutation-breakpoint-disabled")
  );

  ok(
    true,
    "has-mutation class is present, mutation-breakpoint-disabled is not present"
  );
});
