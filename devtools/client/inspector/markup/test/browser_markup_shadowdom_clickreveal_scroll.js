/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that clicking on "reveal" always scrolls the view to show the real container, even
// if the node is already selected.

const TEST_URL = `data:text/html;charset=utf-8,
  <test-component>
    <div slot="slot1" id="el1">slot1 content</div>
  </test-component>

  <script>
    'use strict';
    customElements.define('test-component', class extends HTMLElement {
      constructor() {
        super();
        let shadowRoot = this.attachShadow({mode: 'open'});
        shadowRoot.innerHTML = \`
          <slot name="slot1"></slot>
          <div></div><div></div><div></div><div></div><div></div><div></div>
          <div></div><div></div><div></div><div></div><div></div><div></div>
          <div></div><div></div><div></div><div></div><div></div><div></div>
          <div></div><div></div><div></div><div></div><div></div><div></div>
          <!-- adding some nodes to make sure the slotted container and the real container
           require scrolling -->
        \`;
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
  is(slotChildContainers.length, 1, "Expecting 1 slotted child");

  const slottedContainer = slotChildContainers[0];
  const realContainer = inspector.markup.getContainer(slottedContainer.node);
  const slottedElement = slottedContainer.elt;
  const realElement = realContainer.elt;

  info("Click on the reveal link");
  await clickOnRevealLink(inspector, slottedContainer);
  // "new-node-front" will also trigger the scroll, so make sure we are testing after
  // the scroll was performed.
  await waitUntil(() => isScrolledOut(slottedElement));
  is(isScrolledOut(slottedElement), true, "slotted element is scrolled out");
  is(isScrolledOut(realElement), false, "real element is not scrolled out");

  info("Scroll back to see the slotted element");
  slottedElement.scrollIntoView();
  is(isScrolledOut(slottedElement), false, "slotted element is not scrolled out");
  is(isScrolledOut(realElement), true, "real element is scrolled out");

  info("Click on the reveal link again");
  await clickOnRevealLink(inspector, slottedContainer);
  await waitUntil(() => isScrolledOut(slottedElement));
  is(isScrolledOut(slottedElement), true, "slotted element is scrolled out");
  is(isScrolledOut(realElement), false, "real element is not scrolled out");
});

function isScrolledOut(element) {
  const win = element.ownerGlobal;
  const rect = element.getBoundingClientRect();
  return rect.top < 0 || (rect.top + rect.height) > win.innerHeight;
}
