/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that the breadcrumbs widget refreshes correctly when there are markup
// mutations, even if the currently selected node is a slotted node in the shadow DOM.

const TEST_URL = `data:text/html;charset=utf-8,
  <test-component>
    <div slot="slot1" id="el1">content</div>
  </test-component>

  <script>
    'use strict';
    customElements.define('test-component', class extends HTMLElement {
      constructor() {
        super();
        let shadowRoot = this.attachShadow({mode: 'open'});
        shadowRoot.innerHTML = '<slot class="slot-class" name="slot1"></slot>';
      }
    });
  </script>`;

add_task(async function() {
  await pushPref("dom.webcomponents.shadowdom.enabled", true);
  await pushPref("dom.webcomponents.customelements.enabled", true);

  let {inspector} = await openInspectorForURL(TEST_URL);
  let {markup} = inspector;
  let breadcrumbs = inspector.panelDoc.getElementById("inspector-breadcrumbs");

  info("Find and expand the test-component shadow DOM host.");
  let hostFront = await getNodeFront("test-component", inspector);
  let hostContainer = markup.getContainer(hostFront);
  await expandContainer(inspector, hostContainer);

  info("Expand the shadow root");
  let shadowRootContainer = hostContainer.getChildContainers()[0];
  await expandContainer(inspector, shadowRootContainer);

  let slotContainer = shadowRootContainer.getChildContainers()[0];

  info("Select the slot node and wait for the breadcrumbs update");
  let slotNodeFront = slotContainer.node;
  let onBreadcrumbsUpdated = inspector.once("breadcrumbs-updated");
  inspector.selection.setNodeFront(slotNodeFront);
  await onBreadcrumbsUpdated;

  checkBreadcrumbsContent(breadcrumbs,
    ["html", "body", "test-component", "slot.slot-class"]);

  info("Expand the slot");
  await expandContainer(inspector, slotContainer);

  let slotChildContainers = slotContainer.getChildContainers();
  is(slotChildContainers.length, 1, "Expecting 1 slotted child");

  info("Select the slotted node and wait for the breadcrumbs update");
  let slottedNodeFront = slotChildContainers[0].node;
  onBreadcrumbsUpdated = inspector.once("breadcrumbs-updated");
  inspector.selection.setNodeFront(slottedNodeFront);
  await onBreadcrumbsUpdated;

  checkBreadcrumbsContent(breadcrumbs, ["html", "body", "test-component", "div#el1"]);

  info("Update the classname of the real element and wait for the breadcrumbs update");
  onBreadcrumbsUpdated = inspector.once("breadcrumbs-updated");
  await ContentTask.spawn(gBrowser.selectedBrowser, {}, function() {
    content.document.getElementById("el1").setAttribute("class", "test");
  });
  await onBreadcrumbsUpdated;

  checkBreadcrumbsContent(breadcrumbs,
    ["html", "body", "test-component", "div#el1.test"]);
});

function checkBreadcrumbsContent(breadcrumbs, selectors) {
  info("Check the output of the breadcrumbs widget");
  let container = breadcrumbs.querySelector(".html-arrowscrollbox-inner");
  is(container.childNodes.length, selectors.length, "Correct number of buttons");
  for (let i = 0; i < container.childNodes.length; i++) {
    is(container.childNodes[i].textContent, selectors[i],
      "Text content for button " + i + " is correct");
  }
}
