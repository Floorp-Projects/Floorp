/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(1);

// Test a few static pages using webcomponents with ::marker and ::before
// pseudos and check that they are displayed as expected in the markup view.

const TEST_DATA = [
  {
    // Test that ::before on an empty shadow host is displayed when the host
    // has a ::marker.
    title: "::before after ::marker, empty node",
    url: `data:text/html;charset=utf-8,
      <style>
        test-component { display: list-item; }
        test-component::before { content: "before-host" }
      </style>

      <test-component></test-component>

      <script>
        'use strict';
        customElements.define('test-component', class extends HTMLElement {
          constructor() {
            super();
            let shadowRoot = this.attachShadow({mode: "#MODE#"});
          }
        });
      </script>`,
    tree: `
      test-component
        #shadow-root
        ::marker
        ::before`,

  }, {
    // Test ::before on a shadow host with content is displayed when the host
    // has a ::marker.
    title: "::before after ::marker, non-empty node",
    url: `data:text/html;charset=utf-8,
      <style>
        test-component { display: list-item }
        test-component::before { content: "before-host" }
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
            shadowRoot.innerHTML = "<slot>default content</slot>";
          }
        });
      </script>`,
    tree: `
      test-component
        #shadow-root
          slot
            div!slotted
        ::marker
        ::before
        class="light-dom"`,
  }, {
    // Test just ::marker on a shadow host
    title: "just ::marker, no ::before",
    url: `data:text/html;charset=utf-8,
      <style>
        test-component { display: list-item }
      </style>

      <test-component></test-component>

      <script>
        "use strict";
        customElements.define("test-component", class extends HTMLElement {
          constructor() {
            super();
            let shadowRoot = this.attachShadow({mode: "#MODE#"});
          }
        });
      </script>`,
    tree: `
      test-component
        #shadow-root
        ::marker`,
  },
];

for (const {url, tree, title} of TEST_DATA) {
  // Test each configuration in both open and closed modes
  add_task(async function() {
    info(`Testing: [${title}] in OPEN mode`);
    const {inspector, tab} = await openInspectorForURL(url.replace(/#MODE#/g, "open"));
    await assertMarkupViewAsTree(tree, "test-component", inspector);
    await removeTab(tab);
  });
  add_task(async function() {
    info(`Testing: [${title}] in CLOSED mode`);
    const {inspector, tab} = await openInspectorForURL(url.replace(/#MODE#/g, "closed"));
    await assertMarkupViewAsTree(tree, "test-component", inspector);
    await removeTab(tab);
  });
}
