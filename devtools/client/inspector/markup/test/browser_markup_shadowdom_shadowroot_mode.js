/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the shadow root mode is displayed properly

const TEST_URL = `data:text/html;charset=utf-8,
  <closed-component></closed-component>
  <open-component></open-component>

  <script>
    'use strict';

    customElements.define("closed-component", class extends HTMLElement {
      constructor() {
        super();
        this.attachShadow({mode: "closed"});
      }
    });

    customElements.define("open-component", class extends HTMLElement {
      constructor() {
        super();
        this.attachShadow({ mode: "open" });
      }
    });
  </script>
`;

add_task(async function() {
  await enableWebComponents();

  const { inspector } = await openInspectorForURL(TEST_URL);
  const {markup} = inspector;

  info("Find and expand the closed-component shadow DOM host.");
  const closedHostFront = await getNodeFront("closed-component", inspector);
  const closedHostContainer = markup.getContainer(closedHostFront);
  await expandContainer(inspector, closedHostContainer);

  info("Check the shadow root mode");
  const closedShadowRootContainer = closedHostContainer.getChildContainers()[0];
  assertContainerHasText(closedShadowRootContainer, "#shadow-root (closed)");

  info("Find and expand the open-component shadow DOM host.");
  const openHostFront = await getNodeFront("open-component", inspector);
  const openHostContainer = markup.getContainer(openHostFront);
  await expandContainer(inspector, openHostContainer);

  info("Check the shadow root mode");
  const openShadowRootContainer = openHostContainer.getChildContainers()[0];
  assertContainerHasText(openShadowRootContainer, "#shadow-root (open)");
});
