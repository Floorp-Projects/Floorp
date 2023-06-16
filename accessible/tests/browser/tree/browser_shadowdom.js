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

addAccessibleTask(snippet, async function (browser, accDoc) {
  let container = findAccessibleChildByID(accDoc, "container");

  testChildrenIds(container, ["l1"]);

  await contentSpawnMutation(browser, REORDER, function () {
    let labelEl = content.document.createElement("label");
    labelEl.id = "l2";

    let containerEl = content.document.getElementById("container");
    containerEl.appendChild(labelEl);
  });

  testChildrenIds(container, ["l1", "l2"]);
});

// Dynamically inserted not accessible custom element containing an accessible
// in its shadow DOM.
const snippet2 = `
<script>
customElements.define("x-el2", class extends HTMLElement {
  constructor() {
    super();
    this.attachShadow({ mode: "open" });
    this.shadowRoot.innerHTML = "<input id='input'>";
  }
});
</script>
<div role="group" id="container"></div>
`;

addAccessibleTask(snippet2, async function (browser, accDoc) {
  let container = findAccessibleChildByID(accDoc, "container");

  await contentSpawnMutation(browser, REORDER, function () {
    content.document.getElementById("container").innerHTML = "<x-el2></x-el2>";
  });

  testChildrenIds(container, ["input"]);
});

/**
 * Ensure that changing the slot on the body while moving the body doesn't
 * try to remove the DocAccessible. We test this here instead of in
 * accessible/tests/mochitest/treeupdate/test_shadow_slots.html because this
 * messes with the body element and we don't want that to impact other tests.
 */
addAccessibleTask(
  `
<div id="host"></div>
<script>
  const host = document.getElementById("host");
  host.attachShadow({ mode: "open" });
  const emptyScript = document.createElement("script");
  emptyScript.id = "emptyScript";
  document.head.append(emptyScript);
</script>
  `,
  async function (browser, docAcc) {
    info("Moving body and setting slot on body");
    let reordered = waitForEvent(EVENT_REORDER, docAcc);
    await invokeContentTask(browser, [], () => {
      const host = content.document.getElementById("host");
      const emptyScript = content.document.getElementById("emptyScript");
      const body = content.document.body;
      emptyScript.append(host);
      host.append(body);
      body.slot = "";
    });
    await reordered;
    is(docAcc.childCount, 0, "document has no children after body move");
  },
  { chrome: true, topLevel: true, iframe: true, remoteIframe: true }
);
