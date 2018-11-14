/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the markup-view navigation works correctly with shadow dom slotted nodes.
// Each slotted nodes has two containers representing the same node front in the markup
// view, we need to make sure that navigating to the slotted version selects the slotted
// container, and navigating to the non-slotted element selects the non-slotted container.

const TEST_URL = `data:text/html;charset=utf-8,
  <test-component class="test-component">
    <div slot="slot1" class="slotted1"><div class="slot1-child">slot1-1</div></div>
    <div slot="slot1" class="slotted2">slot1-2</div>
  </test-component>

  <script>
    'use strict';
    customElements.define('test-component', class extends HTMLElement {
      constructor() {
        super();
        let shadowRoot = this.attachShadow({mode: 'open'});
        shadowRoot.innerHTML = '<slot class="slot1" name="slot1"></slot>';
      }
    });
  </script>`;

const TEST_DATA = [
  ["KEY_PageUp", "html"],
  ["KEY_ArrowDown", "head"],
  ["KEY_ArrowDown", "body"],
  ["KEY_ArrowDown", "test-component"],
  ["KEY_ArrowRight", "test-component"],
  ["KEY_ArrowDown", "shadow-root"],
  ["KEY_ArrowRight", "shadow-root"],
  ["KEY_ArrowDown", "slot1"],
  ["KEY_ArrowRight", "slot1"],
  ["KEY_ArrowDown", "div", "slotted1"],
  ["KEY_ArrowDown", "div", "slotted2"],
  ["KEY_ArrowDown", "slotted1"],
  ["KEY_ArrowRight", "slotted1"],
  ["KEY_ArrowDown", "slot1-child"],
  ["KEY_ArrowDown", "slotted2"],
];

add_task(async function() {
  await pushPref("dom.webcomponents.shadowdom.enabled", true);
  await pushPref("dom.webcomponents.customelements.enabled", true);

  const {inspector} = await openInspectorForURL(TEST_URL);

  info("Making sure the markup-view frame is focused");
  inspector.markup._frame.focus();

  info("Starting to iterate through the test data");
  for (const [key, expected, slottedClassName] of TEST_DATA) {
    info("Testing step: " + key + " to navigate to " + expected);
    EventUtils.synthesizeKey(key);

    info("Making sure markup-view children get updated");
    await waitForChildrenUpdated(inspector);

    info("Checking the right node is selected");
    checkSelectedNode(key, expected, slottedClassName, inspector);
  }

  // Same as in browser_markup_navigation.js, use a single catch-call event listener.
  await inspector.once("inspector-updated");
});

function checkSelectedNode(key, expected, slottedClassName, inspector) {
  const selectedContainer = inspector.markup.getSelectedContainer();
  const slotted = !!slottedClassName;

  is(selectedContainer.isSlotted(), slotted,
    `Selected container is ${slotted ? "slotted" : "not slotted"} as expected`);
  is(inspector.selection.isSlotted(), slotted,
    `Inspector selection is also ${slotted ? "slotted" : "not slotted"}`);
  ok(selectedContainer.elt.textContent.includes(expected),
    "Found expected content: " + expected + " in container after pressing " + key);

  if (slotted) {
    is(selectedContainer.node.className, slottedClassName,
      "Slotted has the expected classname " + slottedClassName);
  }
}
