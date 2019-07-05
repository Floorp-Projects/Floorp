/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const REORDER = { expected: [[EVENT_REORDER, "container"]] };

// Dynamically inserted slotted accessible elements should be in
// the accessible tree.
const snippet = `
<script>
customElements.define("x-el", class extends HTMLElement {
  constructor() {
    super();
    this.attachShadow({ mode: "open" });
    this.shadowRoot.innerHTML =
      "<div role='presentation'><slot></slot></div>";
  }
});
</script>
<x-el id="container" role="group"><label id="l1">label1</label></x-el>
`;
addAccessibleTask(snippet, async function(browser, accDoc) {
  let container = findAccessibleChildByID(accDoc, "container");

  testChildrenIds(container, ["l1"]);

  await contentSpawnMutation(browser, REORDER, function() {
    let labelEl = content.document.createElement("label");
    labelEl.id = "l2";

    let containerEl = content.document.getElementById("container");
    containerEl.appendChild(labelEl);
  });

  testChildrenIds(container, ["l1", "l2"]);
});
