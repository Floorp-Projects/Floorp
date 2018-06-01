/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

/* import-globals-from helper_shadowdom.js */

"use strict";

loadHelperScript("helper_shadowdom.js");

// Test that slot elements are correctly updated when slotted elements are being removed
// from the DOM.

const TEST_URL = `data:text/html;charset=utf-8,
  <test-component>
    <div slot="slot1" id="el1">slot1-1</div>
    <div slot="slot1" id="el2">slot1-2</div>
  </test-component>

  <script>
    'use strict';
    customElements.define('test-component', class extends HTMLElement {
      constructor() {
        super();
        let shadowRoot = this.attachShadow({mode: 'open'});
        shadowRoot.innerHTML = '<slot name="slot1"><div>default</div></slot>';
      }
    });
  </script>`;

add_task(async function() {
  await enableWebComponents();

  const {inspector} = await openInspectorForURL(TEST_URL);

  // <test-component> is a shadow host.
  info("Find and expand the test-component shadow DOM host.");
  const hostFront = await getNodeFront("test-component", inspector);
  await inspector.markup.expandNode(hostFront);
  await waitForMultipleChildrenUpdates(inspector);

  info("Test that expanding a shadow host shows shadow root and direct host children.");
  const {markup} = inspector;
  const hostContainer = markup.getContainer(hostFront);
  const childContainers = hostContainer.getChildContainers();

  is(childContainers.length, 3, "Expecting 3 children: shadowroot, 2 host children");
  checkText(childContainers[0], "#shadow-root");
  checkText(childContainers[1], "div");
  checkText(childContainers[2], "div");

  info("Expand the shadow root");
  const shadowRootContainer = childContainers[0];
  await expandContainer(inspector, shadowRootContainer);

  const shadowChildContainers = shadowRootContainer.getChildContainers();
  is(shadowChildContainers.length, 1, "Expecting 1 child slot");
  checkText(shadowChildContainers[0], "slot");

  info("Expand the slot");
  const slotContainer = shadowChildContainers[0];
  await expandContainer(inspector, slotContainer);

  let slotChildContainers = slotContainer.getChildContainers();
  is(slotChildContainers.length, 2, "Expecting 2 slotted children");
  slotChildContainers.forEach(container => checkSlotted(container));

  await deleteNode(inspector, "#el1");
  slotChildContainers = slotContainer.getChildContainers();
  is(slotChildContainers.length, 1, "Expecting 1 slotted child");
  checkSlotted(slotChildContainers[0]);

  await deleteNode(inspector, "#el2");
  slotChildContainers = slotContainer.getChildContainers();
  // After deleting the last host direct child we expect the slot to show the default
  // content <div>default</div>
  is(slotChildContainers.length, 1, "Expecting 1 child");
  ok(!slotChildContainers[0].isSlotted(), "Container is a not slotted container");
});

async function deleteNode(inspector, selector) {
  info("Select node " + selector + " and make sure it is focused");
  await selectNode(selector, inspector);
  await clickContainer(selector, inspector);

  info("Delete the node");
  const mutated = inspector.once("markupmutation");
  const updated = inspector.once("inspector-updated");
  EventUtils.sendKey("delete", inspector.panelWin);
  await mutated;
  await updated;
}
