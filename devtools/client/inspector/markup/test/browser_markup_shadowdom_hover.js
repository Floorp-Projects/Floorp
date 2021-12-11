/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Bug 1465873
// Tests that hovering nodes in the content page with the element picked and finally
// picking one does not break the markup view. The markup and sequence used here is a bit
// eccentric but the issue from Bug 1465873 is tricky to reproduce.

const TEST_URL =
  `data:text/html;charset=utf-8,` +
  encodeURIComponent(`
  <test-component id="component1" background>
    <div slot="slot1" data-index="1">slot1-1</div>
  </test-component>
  <script>
  (function() {
    'use strict';

    customElements.define('test-component', class extends HTMLElement {
      constructor() {
        super(); // always call super() first in the ctor.

        let shadowRoot = this.attachShadow({mode: 'open'});
        shadowRoot.innerHTML = \`
          <div id="wrapper" style="padding-top: 20px;">
            a<span class="pick-target">pick-target</span>
            <div id="slot1-container">
              <slot id="slot1" name="slot1"></slot>
            </div>
          </div>
        \`;
      }
    });
  })();
  </script>`);

add_task(async function() {
  const { inspector, toolbox } = await openInspectorForURL(TEST_URL);

  info("Waiting for element picker to become active.");
  await startPicker(toolbox);

  info("Move mouse over the padding of the test-component");
  await hoverElement(inspector, "test-component", 10, 10);

  info("Move mouse over the pick-target");
  // Note we can't reach pick-target with a selector because this element lives in the
  // shadow-dom of test-component. We aim for PADDING + 5 pixels
  await hoverElement(inspector, "test-component", 10, 25);

  info("Click and pick the pick-target");
  await pickElement(inspector, "test-component", 10, 25);

  info(
    "Check that the markup view has the expected content after using the picker"
  );
  const tree = `
    test-component
      #shadow-root
        wrapper
          a
          pick-target
          slot1-container
            slot1
              div!slotted
      div`;
  await assertMarkupViewAsTree(tree, "test-component", inspector);

  const hostFront = await getNodeFront("test-component", inspector);
  const hostContainer = inspector.markup.getContainer(hostFront);
  const moreNodesLink = hostContainer.elt.querySelector(".more-nodes");
  ok(
    !moreNodesLink,
    "There is no 'more nodes' button displayed in the host container"
  );
});
