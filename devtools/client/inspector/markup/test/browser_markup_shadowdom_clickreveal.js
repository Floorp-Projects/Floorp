/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

/* import-globals-from helper_shadowdom.js */

"use strict";

loadHelperScript("helper_shadowdom.js");

// Test that the corresponding non-slotted node container gets selected when clicking on
// the reveal link for a slotted node.

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
        shadowRoot.innerHTML = '<slot name="slot1"></slot>';
      }
    });
  </script>`;

add_task(async function() {
  await enableWebComponents();

  const {inspector} = await openInspectorForURL(TEST_URL);
  const {markup} = inspector;

  info("Find and expand the test-component shadow DOM host.");
  const hostFront = await getNodeFront("test-component", inspector);
  const hostContainer = markup.getContainer(hostFront);
  await expandContainer(inspector, hostContainer);

  info("Expand the shadow root");
  const shadowRootContainer = hostContainer.getChildContainers()[0];
  await expandContainer(inspector, shadowRootContainer);

  info("Expand the slot");
  const slotContainer = shadowRootContainer.getChildContainers()[0];
  await expandContainer(inspector, slotContainer);

  const slotChildContainers = slotContainer.getChildContainers();
  is(slotChildContainers.length, 2, "Expecting 2 slotted children");

  await checkRevealLink(inspector, slotChildContainers[0].node);
  is(inspector.selection.nodeFront.id, "el1", "The right node was selected");
  is(hostContainer.getChildContainers()[1].node, inspector.selection.nodeFront);

  await checkRevealLink(inspector, slotChildContainers[1].node);
  is(inspector.selection.nodeFront.id, "el2", "The right node was selected");
  is(hostContainer.getChildContainers()[2].node, inspector.selection.nodeFront);
});

async function checkRevealLink(inspector, node) {
  const slottedContainer = inspector.markup.getContainer(node, true);
  info("Select the slotted container for the element");
  await selectNode(node, inspector, "no-reason", true);
  ok(inspector.selection.isSlotted(), "The selection is the slotted version");
  ok(inspector.markup.getSelectedContainer().isSlotted(),
    "The selected container is slotted");

  info("Click on the reveal link and wait for the new node to be selected");
  await clickOnRevealLink(inspector, slottedContainer);
  const selectedFront = inspector.selection.nodeFront;
  is(selectedFront, node, "The same node front is still selected");
  ok(!inspector.selection.isSlotted(), "The selection is not the slotted version");
  ok(!inspector.markup.getSelectedContainer().isSlotted(),
    "The selected container is not slotted");
}

async function clickOnRevealLink(inspector, container) {
  const onSelection = inspector.selection.once("new-node-front");
  const revealLink = container.elt.querySelector(".reveal-link");
  const tagline = revealLink.closest(".tag-line");
  const win = inspector.markup.doc.defaultView;

  // First send a mouseover on the tagline to force the link to be displayed.
  EventUtils.synthesizeMouseAtCenter(tagline, {type: "mouseover"}, win);
  EventUtils.synthesizeMouseAtCenter(revealLink, {}, win);

  await onSelection;
}
