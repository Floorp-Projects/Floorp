/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test inspector markup view handling DOM mutation breakpoints icons
// The icon should display when a breakpoint exists for a given node

function toggleMutationBreakpoint(inspector) {
  const allMenuItems = openContextMenuAndGetAllItems(inspector);
  const attributeMenuItem = allMenuItems.find(
    ({ id }) => id === "node-menu-mutation-breakpoint-attribute"
  );
  attributeMenuItem.click();
}

async function waitForMutationMarkerStyle(win, node, value) {
  return waitFor(() => win.getComputedStyle(node).display === value);
}

add_task(async function() {
  const { inspector } = await openInspectorForURL(
    "data:text/html;charset=utf-8,<h1>foo</h1><span>bar</span>"
  );
  const { win } = inspector.markup;

  await selectNode("span", inspector);
  toggleMutationBreakpoint(inspector);

  const span = await getContainerForSelector("span", inspector);
  const mutationMarker = span.tagLine.querySelector(
    ".markup-tag-mutation-marker"
  );
  await waitForMutationMarkerStyle(win, mutationMarker, "block");
  ok(true, "DOM Mutation marker is displaying");

  toggleMutationBreakpoint(inspector);
  await waitForMutationMarkerStyle(win, mutationMarker, "none");
  ok(true, "DOM Mutation marker is hidden");
});
