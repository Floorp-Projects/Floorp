/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the "Show all 'N' nodes" button displays the proper value

const NODE_COUNT = 101;
const TEST_URL = `data:text/html;charset=utf-8,
  <test-component>
  </test-component>

  <script>
    'use strict';
    for (let i = 0; i < ${NODE_COUNT}; i++) {
      const div = document.createElement("div");
      div.innerHTML = i;
      document.querySelector('test-component').appendChild(div);
    }
    customElements.define('test-component', class extends HTMLElement {
      constructor() {
        super();
        let shadowRoot = this.attachShadow({mode: 'open'});
        shadowRoot.innerHTML = '<slot></slot>';
      }
    });
  </script>`;

add_task(async function () {
  const { inspector } = await openInspectorForURL(TEST_URL);
  const { markup } = inspector;

  info("Find and expand the component shadow DOM host.");
  const hostFront = await getNodeFront("test-component", inspector);
  const hostContainer = markup.getContainer(hostFront);
  await expandContainer(inspector, hostContainer);
  const shadowRootContainer = hostContainer.getChildContainers()[0];
  await expandContainer(inspector, shadowRootContainer);

  info("Expand the slot");
  const slotContainer = shadowRootContainer.getChildContainers()[0];
  await expandContainer(inspector, slotContainer);

  info("Find the 'Show all nodes' button");
  const button = slotContainer.elt.querySelector(
    "button:not(.inspector-badge)"
  );
  ok(
    button.innerText.includes(NODE_COUNT),
    `'Show all nodes' button contains correct node count (expected "${button.innerText}" to include "${NODE_COUNT}")`
  );
});
