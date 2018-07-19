/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that using copyCssPath, copyXPath and copyUniqueSelector with an element under a
// shadow root returns values relevant to the selected element, and relative to the shadow
// root.

const TEST_URL = `data:text/html;charset=utf-8,
  <test-component></test-component>

  <script>
    'use strict';
    customElements.define('test-component', class extends HTMLElement {
      constructor() {
        super();
        let shadowRoot = this.attachShadow({mode: 'open'});
        shadowRoot.innerHTML = \`
          <div id="el1">
            <span></span>
            <span></span>
            <span></span>
          </div>\`;
      }
    });
  </script>`;

add_task(async function() {
  await enableWebComponents();

  const {inspector} = await openInspectorForURL(TEST_URL);
  const {markup} = inspector;

  info("Find and expand the test-component shadow DOM host.");
  const hostFront = await getNodeFront("test-component", inspector);
  const hostContainer = markup.getContainer(hostFront);
  await expandContainer(inspector, hostContainer);

  info("Expand the shadow root");
  const shadowRootContainer = hostContainer.getChildContainers()[0];
  await expandContainer(inspector, shadowRootContainer);

  info("Select the div under the shadow root");
  const divContainer = shadowRootContainer.getChildContainers()[0];
  await selectNode(divContainer.node, inspector);

  info("Check the copied values for the various copy*Path helpers");
  await waitForClipboardPromise(() => inspector.copyXPath(), '//*[@id="el1"]');
  await waitForClipboardPromise(() => inspector.copyCssPath(), "div#el1");
  await waitForClipboardPromise(() => inspector.copyUniqueSelector(), "#el1");

  info("Expand the div");
  await expandContainer(inspector, divContainer);

  info("Select the third span");
  const spanContainer = divContainer.getChildContainers()[2];
  await selectNode(spanContainer.node, inspector);

  info("Check the copied values for the various copy*Path helpers");
  await waitForClipboardPromise(() => inspector.copyXPath(), "/div/span[3]");
  await waitForClipboardPromise(() => inspector.copyCssPath(), "div#el1 span");
  await waitForClipboardPromise(() => inspector.copyUniqueSelector(),
    "#el1 > span:nth-child(3)");
});
