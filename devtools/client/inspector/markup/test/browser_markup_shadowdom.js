/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

// Test a few static pages using webcomponents and check that they are displayed as
// expected in the markup view.

const TEST_DATA = [
  {
    // Test that expanding a shadow host shows a shadow root node and direct children.
    // Test that expanding a shadow root shows the shadow dom.
    // Test that slotted elements are visible in the shadow dom.
    title: "generic shadow dom test",
    url: `data:text/html;charset=utf-8,
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
            let shadowRoot = this.attachShadow({mode: "#MODE#"});
            shadowRoot.innerHTML = \`
                <slot name="slot1"></slot>
                <slot name="slot2"></slot>
                <slot></slot>
              \`;
          }
        });
      </script>`,
    tree: `
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
          inner`

  }, {
    // Test that components without any direct children still display a shadow root node,
    // if a shadow root is attached to the host.
    title: "shadow root without direct children",
    url: `data:text/html;charset=utf-8,
      <test-component></test-component>
      <script>
        "use strict";
        customElements.define("test-component", class extends HTMLElement {
          constructor() {
            super();
            let shadowRoot = this.attachShadow({mode: "#MODE#"});
            shadowRoot.innerHTML = "<slot><div>fallback-content</div></slot>";
          }
        });
      </script>`,
    tree: `
      test-component
        #shadow-root
          slot
            fallback-content`

  }, {
    // Test that markup view is correctly displayed for non-trivial shadow DOM nesting.
    title: "nested components",
    url: `data:text/html;charset=utf-8,
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
              let shadowRoot = this.attachShadow({mode: "#MODE#"});
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
      </script>`,
    tree: `
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
            div`

  }, {
    // Test that ::before and ::after pseudo elements are correctly displayed in host
    // components and in slot elements.
    title: "pseudo elements",
    url: `data:text/html;charset=utf-8,
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
            let shadowRoot = this.attachShadow({mode: "#MODE#"});
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
      </script>`,
    tree: `
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
        ::after`

  }, {
    // Test empty web components are still displayed correctly.
    title: "empty components",
    url: `data:text/html;charset=utf-8,
      <test-component></test-component>

      <script>
        "use strict";
        customElements.define("test-component", class extends HTMLElement {
          constructor() {
            super();
            let shadowRoot = this.attachShadow({mode: "#MODE#"});
            shadowRoot.innerHTML = "";
          }
        });
      </script>`,
    tree: `
      test-component
        #shadow-root`
  }
];

for (const {url, tree, title} of TEST_DATA) {
  // Test each configuration in both open and closed modes
  add_task(async function() {
    info(`Testing: [${title}] in OPEN mode`);
    await enableWebComponents();
    const {inspector} = await openInspectorForURL(url.replace("#MODE#", "open"));
    await assertMarkupViewAsTree(tree, "test-component", inspector);
  });
  add_task(async function() {
    info(`Testing: [${title}] in CLOSED mode`);
    await enableWebComponents();
    const {inspector} = await openInspectorForURL(url.replace("#MODE#", "closed"));
    await assertMarkupViewAsTree(tree, "test-component", inspector);
  });
}
