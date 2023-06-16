/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URL = `data:text/html;charset=utf8,
  <test-component></test-component>
  <script>
    'use strict';
    customElements.define('test-component', class extends HTMLElement {
      constructor() {
        super();
        let shadowRoot = this.attachShadow({mode: 'open'});
        shadowRoot.innerHTML =
          '<div style="width:30px;height:30px;background:rgb(0, 128, 0)"></div>';
      }
    });
  </script>`;

// Test that the "Screenshot Node" feature works with a node inside a shadow root.
add_task(async function () {
  const { inspector, toolbox } = await openInspectorForURL(encodeURI(TEST_URL));

  info("Select the green node");
  const greenNode = await getNodeFrontInShadowDom(
    "div",
    "test-component",
    inspector
  );
  await selectNode(greenNode, inspector);

  info("Take a screenshot of the green node and verify it looks as expected");
  const greenScreenshot = await takeNodeScreenshot(inspector);
  await assertSingleColorScreenshotImage(greenScreenshot, 30, 30, {
    r: 0,
    g: 128,
    b: 0,
  });

  await toolbox.destroy();
});
