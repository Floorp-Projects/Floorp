/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the inspector is correctly updated when shadow roots are attached to
// components after displaying them in the markup view.

const TEST_URL = `data:text/html;charset=utf-8,` + encodeURIComponent(`
  <div id="root">
    <test-component>
      <div slot="slot1" id="el1">slot1-1</div>
      <div slot="slot1" id="el2">slot1-2</div>
    </test-component>
    <inline-component>inline text</inline-component>
  </div>

  <script>
    'use strict';
    window.attachTestComponent = function () {
      customElements.define('test-component', class extends HTMLElement {
        constructor() {
          super();
          let shadowRoot = this.attachShadow({mode: 'open'});
          shadowRoot.innerHTML = \`<div id="slot1-container">
                                     <slot name="slot1"></slot>
                                   </div>
                                   <other-component>
                                     <div slot="slot2">slot2-1</div>
                                   </other-component>\`;
        }
      });
    }

    window.attachOtherComponent = function () {
      customElements.define('other-component', class extends HTMLElement {
        constructor() {
          super();
          let shadowRoot = this.attachShadow({mode: 'open'});
          shadowRoot.innerHTML = \`<div id="slot2-container">
                                     <slot name="slot2"></slot>
                                     <div>some-other-node</div>
                                   </div>\`;
        }
      });
    }

    window.attachInlineComponent = function () {
      customElements.define('inline-component', class extends HTMLElement {
        constructor() {
          super();
          let shadowRoot = this.attachShadow({mode: 'open'});
          shadowRoot.innerHTML = \`<div id="inline-component-content">
                                     <div>some-inline-content</div>
                                   </div>\`;
        }
      });
    }
  </script>`);

add_task(async function() {
  await enableWebComponents();

  const {inspector} = await openInspectorForURL(TEST_URL);

  const tree = `
    div
      test-component
        slot1-1
        slot1-2
      inline text`;
  await assertMarkupViewAsTree(tree, "#root", inspector);

  info("Attach a shadow root to test-component");
  let mutated = waitForMutation(inspector, "shadowRootAttached");
  ContentTask.spawn(gBrowser.selectedBrowser, {}, function() {
    content.wrappedJSObject.attachTestComponent();
  });
  await mutated;

  const treeAfterTestAttach = `
    div
      test-component
        #shadow-root
          slot1-container
            slot
              div!slotted
              div!slotted
          other-component
            slot2-1
        slot1-1
        slot1-2
      inline text`;
  await assertMarkupViewAsTree(treeAfterTestAttach, "#root", inspector);

  info("Attach a shadow root to other-component, nested in test-component");
  mutated = waitForMutation(inspector, "shadowRootAttached");
  ContentTask.spawn(gBrowser.selectedBrowser, {}, function() {
    content.wrappedJSObject.attachOtherComponent();
  });
  await mutated;

  const treeAfterOtherAttach = `
    div
      test-component
        #shadow-root
          slot1-container
            slot
              div!slotted
              div!slotted
          other-component
            #shadow-root
              slot2-container
                slot
                  div!slotted
                some-other-node
            slot2-1
        slot1-1
        slot1-2
      inline text`;
  await assertMarkupViewAsTree(treeAfterOtherAttach, "#root", inspector);

  info("Attach a shadow root to inline-component, check the inline text child.");
  mutated = waitForMutation(inspector, "shadowRootAttached");
  ContentTask.spawn(gBrowser.selectedBrowser, {}, function() {
    content.wrappedJSObject.attachInlineComponent();
  });
  await mutated;

  const treeAfterInlineAttach = `
    div
      test-component
        #shadow-root
          slot1-container
            slot
              div!slotted
              div!slotted
          other-component
            #shadow-root
              slot2-container
                slot
                  div!slotted
                some-other-node
            slot2-1
        slot1-1
        slot1-2
      inline-component
        #shadow-root
          inline-component-content
            some-inline-content
        inline text`;
  await assertMarkupViewAsTree(treeAfterInlineAttach, "#root", inspector);
});
