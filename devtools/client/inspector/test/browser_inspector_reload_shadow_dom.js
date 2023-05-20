/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Check that the markup view selection is preserved even if the selection is in shadow-dom.

const HTML = `
    <html>
      <head>
        <meta charset="utf8">
        <title>Test</title>
      </head>
      <body>
        <h1>Shadow DOM test</h1>
        <test-component>
          <div slot="slot1" id="el1">content</div>
        </test-component>
        <script>
          'use strict';

          customElements.define('test-component', class extends HTMLElement {
            constructor() {
              super();
              const shadowRoot = this.attachShadow({mode: 'open'});
              shadowRoot.innerHTML = '<slot class="slot-class" name="slot1"></slot>';
            }
          });
        </script>
      </body>
    </html>
`;

const TEST_URI = "data:text/html;charset=utf-8," + encodeURI(HTML);

add_task(async function () {
  const { inspector } = await openInspectorForURL(TEST_URI);

  info("Select node in shadow DOM");
  const nodeFront = await getNodeFrontInShadowDom(
    "slot",
    "test-component",
    inspector
  );
  await selectNode(nodeFront, inspector);

  info("Reloading page.");
  await navigateTo(TEST_URI);

  const reloadedNodeFront = await getNodeFrontInShadowDom(
    "slot",
    "test-component",
    inspector
  );

  is(
    inspector.selection.nodeFront,
    reloadedNodeFront,
    "<slot> is selected after reload."
  );
});
