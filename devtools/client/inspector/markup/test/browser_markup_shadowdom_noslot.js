/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the markup view is correctly displayed when a component has children but no
// slots are available under the shadow root.

const TEST_URL = `data:text/html;charset=utf-8,
  <style>
    .has-before::before { content: "before-content" }
  </style>

  <div class="root">
    <no-slot-component>
      <div class="not-nested">light</div>
      <div class="nested">
        <div class="has-before"></div>
        <div>dummy for Bug 1441863</div>
      </div>
    </no-slot-component>
    <slot-component>
      <div class="not-nested">light</div>
      <div class="nested">
        <div class="has-before"></div>
      </div>
    </slot-component>
  </div>

  <script>
    'use strict';
    customElements.define('no-slot-component', class extends HTMLElement {
      constructor() {
        super();
        let shadowRoot = this.attachShadow({mode: 'open'});
        shadowRoot.innerHTML = '<div class="no-slot-div"></div>';
      }
    });
    customElements.define('slot-component', class extends HTMLElement {
      constructor() {
        super();
        let shadowRoot = this.attachShadow({mode: 'open'});
        shadowRoot.innerHTML = '<slot></slot>';
      }
    });
  </script>`;

add_task(async function() {
  await enableWebComponents();

  const {inspector} = await openInspectorForURL(TEST_URL);

  // We expect that host children are correctly displayed when no slots are defined.
  const beforeTree = `
  class="root"
    no-slot-component
      #shadow-root
        no-slot-div
      class="not-nested"
      class="nested"
        class="has-before"
        dummy for Bug 1441863
    slot-component
      #shadow-root
        slot
          div!slotted
          div!slotted
      class="not-nested"
      class="nested"
        class="has-before"
          ::before`;
  await assertMarkupViewAsTree(beforeTree, ".root", inspector);

  info("Move the non-slotted element with class has-before and check the pseudo appears");
  const mutated = waitForNMutations(inspector, "childList", 2);
  const pseudoMutated = waitForMutation(inspector, "nativeAnonymousChildList");
  ContentTask.spawn(gBrowser.selectedBrowser, {}, function() {
    const root = content.document.querySelector(".root");
    const hasBeforeEl = content.document.querySelector("no-slot-component .has-before");
    root.appendChild(hasBeforeEl);
  });
  await mutated;
  await pseudoMutated;

  // As the non-slotted has-before is moved into the tree, the before pseudo is expected
  // to appear.
  const afterTree = `
    class="root"
      no-slot-component
        #shadow-root
          no-slot-div
        class="not-nested"
        class="nested"
          dummy for Bug 1441863
      slot-component
        #shadow-root
          slot
            div!slotted
            div!slotted
        class="not-nested"
        class="nested"
          class="has-before"
            ::before
      class="has-before"
        ::before`;
  await assertMarkupViewAsTree(afterTree, ".root", inspector);
});
