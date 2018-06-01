/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

/* import-globals-from helper_shadowdom.js */

"use strict";

loadHelperScript("helper_shadowdom.js");

// Test a few static pages using webcomponents and check that they are displayed as
// expected in the markup view.

add_task(async function() {
  await enableWebComponents();

  // Test that expanding a shadow host shows a shadow root node and direct host children.
  // Test that expanding a shadow root shows the shadow dom.
  // Test that slotted elements are visible in the shadow dom.

  const TEST_URL = `data:text/html;charset=utf-8,
    <test-component>
      <div slot="slot1">slotted-1<div>inner</div></div>
      <div slot="slot2">slotted-2<div>inner</div></div>
      <div class="no-slot-class">no-slot-text<div>inner</div></div>
    </test-component>

    <script>
      'use strict';
      customElements.define('test-component', class extends HTMLElement {
        constructor() {
          super();
          let shadowRoot = this.attachShadow({mode: 'open'});
          shadowRoot.innerHTML = \`
              <slot name="slot1"></slot>
              <slot name="slot2"></slot>
              <slot></slot>
            \`;
        }
      });
    </script>`;

  const EXPECTED_TREE = `
    test-component
      #shadow-root
        name="slot1"
          div!slotted
        name="slot2"
          div!slotted
        slot
          div!slotted
      slot="slot1"
        slotted-1
        inner
      slot="slot2"
        slotted-2
        inner
      class="no-slot-class"
        no-slot-text
        inner`;

  const {inspector} = await openInspectorForURL(TEST_URL);
  await checkTreeFromRootSelector(EXPECTED_TREE, "test-component", inspector);
});

add_task(async function() {
  await enableWebComponents();

  // Test that components without any direct children still display a shadow root node, if
  // a shadow root is attached to the host.

  const TEST_URL = `data:text/html;charset=utf-8,
    <test-component></test-component>
    <script>
      "use strict";
      customElements.define("test-component", class extends HTMLElement {
        constructor() {
          super();
          let shadowRoot = this.attachShadow({mode: "open"});
          shadowRoot.innerHTML = "<slot><div>fallback-content</div></slot>";
        }
      });
    </script>`;

  const EXPECTED_TREE = `
    test-component
      #shadow-root
        slot
          fallback-content`;

  const {inspector} = await openInspectorForURL(TEST_URL);
  await checkTreeFromRootSelector(EXPECTED_TREE, "test-component", inspector);
});

add_task(async function() {
  await enableWebComponents();

  // Test that the markup view is correctly displayed for non-trivial shadow DOM nesting.

  const TEST_URL = `data:text/html;charset=utf-8,
    <test-component >
      <div slot="slot1">slot1-1</div>
      <third-component slot="slot2"></third-component>
    </test-component>

    <script>
    (function() {
      'use strict';

      function defineComponent(name, html) {
        customElements.define(name, class extends HTMLElement {
          constructor() {
            super();
            let shadowRoot = this.attachShadow({mode: 'open'});
            shadowRoot.innerHTML = html;
          }
        });
      }

      defineComponent('test-component', \`
        <div id="test-container">
          <slot name="slot1"></slot>
          <slot name="slot2"></slot>
          <other-component><div slot="other1">other1-content</div></other-component>
        </div>\`);
      defineComponent('other-component',
        '<div id="other-container"><slot id="other1" name="other1"></slot></div>');
      defineComponent('third-component', '<div>Third component</div>');
    })();
    </script>`;

  const EXPECTED_TREE = `
    test-component
      #shadow-root
        test-container
          slot
            div!slotted
          slot
            third-component!slotted
          other-component
            #shadow-root
              div
                slot
                  div!slotted
            div
      div
      third-component
        #shadow-root
          div`;

  const {inspector} = await openInspectorForURL(TEST_URL);
  await checkTreeFromRootSelector(EXPECTED_TREE, "test-component", inspector);
});

add_task(async function() {
  await enableWebComponents();

  // Test that ::before and ::after pseudo elements are correctly displayed in host
  // components and in slot elements.

  const TEST_URL = `data:text/html;charset=utf-8,
    <style>
      test-component::before { content: "before-host" }
      test-component::after { content: "after-host" }
    </style>

    <test-component>
      <div class="light-dom"></div>
    </test-component>

    <script>
      "use strict";
      customElements.define("test-component", class extends HTMLElement {
        constructor() {
          super();
          let shadowRoot = this.attachShadow({mode: "open"});
          shadowRoot.innerHTML = \`
            <style>
              slot { display: block } /* avoid whitespace nodes */
              slot::before { content: "before-slot" }
              slot::after { content: "after-slot" }
            </style>
            <slot>default content</slot>
          \`;
        }
      });
    </script>`;

  const EXPECTED_TREE = `
    test-component
      #shadow-root
        style
          slot { display: block }
        slot
          ::before
          div!slotted
          ::after
      ::before
      class="light-dom"
      ::after`;

  const {inspector} = await openInspectorForURL(TEST_URL);
  await checkTreeFromRootSelector(EXPECTED_TREE, "test-component", inspector);
});

add_task(async function() {
  await enableWebComponents();

  // Test empty web components are still displayed correctly.

  const TEST_URL = `data:text/html;charset=utf-8,
    <test-component></test-component>

    <script>
      "use strict";
      customElements.define("test-component", class extends HTMLElement {
        constructor() {
          super();
          let shadowRoot = this.attachShadow({mode: "open"});
          shadowRoot.innerHTML = "";
        }
      });
    </script>`;

  const EXPECTED_TREE = `
    test-component
      #shadow-root`;

  const {inspector} = await openInspectorForURL(TEST_URL);
  await checkTreeFromRootSelector(EXPECTED_TREE, "test-component", inspector);
});
