/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the markup view properly displays the "more nodes" button both for host
// elements and for slot elements.

const TEST_URL = `data:text/html;charset=utf-8,
<test-component>
  <div>node 1</div><div>node 2</div><div>node 3</div>
  <div>node 4</div><div>node 5</div><div>node 6</div>
</test-component>

<script>
  "use strict";
  customElements.define("test-component", class extends HTMLElement {
    constructor() {
      super();
      let shadowRoot = this.attachShadow({mode: "open"});
      shadowRoot.innerHTML = "<slot>some default content</slot>";
    }
    connectedCallback() {}
    disconnectedCallback() {}
  });
</script>`;

add_task(async function() {
  await enableWebComponents();
  await pushPref("devtools.markup.pagesize", 5);

  const {inspector} = await openInspectorForURL(TEST_URL);

  // <test-component> is a shadow host.
  info("Find and expand the test-component shadow DOM host.");
  const hostFront = await getNodeFront("test-component", inspector);
  await inspector.markup.expandNode(hostFront);
  await waitForMultipleChildrenUpdates(inspector);

  info("Test that expanding a shadow host shows shadow root and direct host children.");
  const {markup} = inspector;
  const hostContainer = markup.getContainer(hostFront);
  let childContainers = hostContainer.getChildContainers();

  is(childContainers.length, 6, "Expecting 6 children: shadowroot, 5 host children");
  assertContainerHasText(childContainers[0], "#shadow-root");
  for (let i = 1; i < 6; i++) {
    assertContainerHasText(childContainers[i], "div");
    assertContainerHasText(childContainers[i], "node " + i);
  }

  info("Click on the more nodes button under the host element");
  let moreNodesLink = hostContainer.elt.querySelector(".more-nodes");
  ok(!!moreNodesLink, "A 'more nodes' button is displayed in the host container");
  moreNodesLink.querySelector("button").click();
  await inspector.markup._waitForChildren();

  childContainers = hostContainer.getChildContainers();
  is(childContainers.length, 7, "Expecting one additional host child");
  assertContainerHasText(childContainers[6], "div");
  assertContainerHasText(childContainers[6], "node 6");

  info("Expand the shadow root");
  const shadowRootContainer = childContainers[0];
  const shadowRootFront = shadowRootContainer.node;
  await inspector.markup.expandNode(shadowRootFront);
  await waitForMultipleChildrenUpdates(inspector);

  const shadowChildContainers = shadowRootContainer.getChildContainers();
  is(shadowChildContainers.length, 1, "Expecting 1 slot child");
  assertContainerHasText(shadowChildContainers[0], "slot");

  info("Expand the slot");
  const slotContainer = shadowChildContainers[0];
  const slotFront = slotContainer.node;
  await inspector.markup.expandNode(slotFront);
  await waitForMultipleChildrenUpdates(inspector);

  let slotChildContainers = slotContainer.getChildContainers();
  is(slotChildContainers.length, 5, "Expecting 5 slotted children");
  for (const slotChildContainer of slotChildContainers) {
    assertContainerHasText(slotChildContainer, "div");
    ok(slotChildContainer.elt.querySelector(".reveal-link"),
      "Slotted container has a reveal link element");
  }

  info("Click on the more nodes button under the slot element");
  moreNodesLink = slotContainer.elt.querySelector(".more-nodes");
  ok(!!moreNodesLink, "A 'more nodes' button is displayed in the host container");
  EventUtils.sendMouseEvent({type: "click"}, moreNodesLink.querySelector("button"));
  await inspector.markup._waitForChildren();

  slotChildContainers = slotContainer.getChildContainers();
  is(slotChildContainers.length, 6, "Expecting one additional slotted element");
  assertContainerHasText(slotChildContainers[5], "div");
  ok(slotChildContainers[5].elt.querySelector(".reveal-link"),
    "Slotted container has a reveal link element");
});
