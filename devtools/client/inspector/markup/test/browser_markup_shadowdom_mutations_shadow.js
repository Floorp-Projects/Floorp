/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the markup view is correctly updated when elements under a shadow root are
// deleted or updated.

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
        shadowRoot.innerHTML = \`<div id="slot1-container">
                                   <slot name="slot1"></slot>
                                 </div>
                                 <div id="another-div"></div>\`;
      }
    });
  </script>`;

add_task(async function() {
  const { inspector } = await openInspectorForURL(TEST_URL);

  const tree = `
    test-component
      #shadow-root
        slot1-container
          slot
            div!slotted
            div!slotted
        another-div
      div
      div`;
  await assertMarkupViewAsTree(tree, "test-component", inspector);

  info("Delete a shadow dom element and check the updated markup view");
  let mutated = waitForMutation(inspector, "childList");
  SpecialPowers.spawn(gBrowser.selectedBrowser, [], function() {
    const shadowRoot = content.document.querySelector("test-component")
      .shadowRoot;
    const slotContainer = shadowRoot.getElementById("slot1-container");
    slotContainer.remove();
  });
  await mutated;

  const treeAfterDelete = `
    test-component
      #shadow-root
        another-div
      div
      div`;
  await assertMarkupViewAsTree(treeAfterDelete, "test-component", inspector);

  mutated = inspector.once("markupmutation");
  SpecialPowers.spawn(gBrowser.selectedBrowser, [], function() {
    const shadowRoot = content.document.querySelector("test-component")
      .shadowRoot;
    const shadowDiv = shadowRoot.getElementById("another-div");
    shadowDiv.setAttribute("random-attribute", "1");
  });
  await mutated;

  info(
    "Add an attribute on a shadow dom element and check the updated markup view"
  );
  const treeAfterAttrChange = `
    test-component
      #shadow-root
        random-attribute
      div
      div`;
  await assertMarkupViewAsTree(
    treeAfterAttrChange,
    "test-component",
    inspector
  );
});
