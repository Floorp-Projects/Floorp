/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

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

// Test reveal link with mouse navigation
add_task(async function() {
  const checkWithMouse = checkRevealLink.bind(null, clickOnRevealLink);
  await testRevealLink(checkWithMouse, checkWithMouse);
});

// Test reveal link with keyboard navigation (Enter and Spacebar keys)
add_task(async function() {
  const checkWithEnter = checkRevealLink.bind(
    null,
    keydownOnRevealLink.bind(null, "KEY_Enter")
  );
  const checkWithSpacebar = checkRevealLink.bind(
    null,
    keydownOnRevealLink.bind(null, " ")
  );

  await testRevealLink(checkWithEnter, checkWithSpacebar);
});

async function testRevealLink(revealFnFirst, revealFnSecond) {
  const { inspector } = await openInspectorForURL(TEST_URL);
  const { markup } = inspector;

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

  await revealFnFirst(inspector, slotChildContainers[0].node);
  is(inspector.selection.nodeFront.id, "el1", "The right node was selected");
  is(hostContainer.getChildContainers()[1].node, inspector.selection.nodeFront);

  await revealFnSecond(inspector, slotChildContainers[1].node);
  is(inspector.selection.nodeFront.id, "el2", "The right node was selected");
  is(hostContainer.getChildContainers()[2].node, inspector.selection.nodeFront);
}

async function checkRevealLink(actionFn, inspector, node) {
  const slottedContainer = inspector.markup.getContainer(node, true);
  info("Select the slotted container for the element");
  await selectNode(node, inspector, "no-reason", true);
  ok(inspector.selection.isSlotted(), "The selection is the slotted version");
  ok(
    inspector.markup.getSelectedContainer().isSlotted(),
    "The selected container is slotted"
  );

  const link = slottedContainer.elt.querySelector(".reveal-link");
  is(
    link.getAttribute("role"),
    "link",
    "Reveal link has the role=link attribute"
  );

  info("Click on the reveal link and wait for the new node to be selected");
  await actionFn(inspector, slottedContainer);
  const selectedFront = inspector.selection.nodeFront;
  is(selectedFront, node, "The same node front is still selected");
  ok(
    !inspector.selection.isSlotted(),
    "The selection is not the slotted version"
  );
  // wait until the selected container isn't the one we had before.
  await waitFor(
    () => inspector.markup.getSelectedContainer() !== slottedContainer
  );
  ok(
    !inspector.markup.getSelectedContainer().isSlotted(),
    "The selected container is not slotted"
  );
}
