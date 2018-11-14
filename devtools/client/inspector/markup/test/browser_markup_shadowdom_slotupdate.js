/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that slotted elements are correctly updated when the slot attribute is modified
// on already slotted elements.

const TEST_URL = `data:text/html;charset=utf-8,
  <test-component>
    <div slot="slot1">slot1-1</div>
    <div slot="slot1">slot1-2</div>
    <div slot="slot2" id="to-update">slot2-1</div>
    <div slot="slot2">slot2-2</div>
  </test-component>

  <script>
    'use strict';
    customElements.define('test-component', class extends HTMLElement {
      constructor() {
        super();
        let shadowRoot = this.attachShadow({mode: 'open'});
        shadowRoot.innerHTML = '<slot name="slot1"></slot><slot name="slot2"></slot>';
      }
    });
  </script>`;

add_task(async function() {
  await enableWebComponents();

  const {inspector} = await openInspectorForURL(TEST_URL);

  const tree = `
    test-component
      #shadow-root
        name="slot1"
          div!slotted
          div!slotted
        name="slot2"
          div!slotted
          div!slotted
      slot1-1
      slot1-2
      slot2-1
      slot2-2`;
  await assertMarkupViewAsTree(tree, "test-component", inspector);

  info("Listening for the markupmutation event");
  const mutated = inspector.once("markupmutation");
  ContentTask.spawn(gBrowser.selectedBrowser, {}, function() {
    content.document.getElementById("to-update").setAttribute("slot", "slot1");
  });
  await mutated;

  // After mutation we expect slot1 to have one more slotted node, and slot2 one less.
  const mutatedTree = `
    test-component
      #shadow-root
        name="slot1"
          div!slotted
          div!slotted
          div!slotted
        name="slot2"
          div!slotted
      slot1-1
      slot1-2
      slot2-1
      slot2-2`;
  await assertMarkupViewAsTree(mutatedTree, "test-component", inspector);
});
