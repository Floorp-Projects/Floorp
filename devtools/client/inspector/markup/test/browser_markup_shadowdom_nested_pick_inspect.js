/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
/* globals getTestActorWithoutToolbox */

"use strict";

// Test that the markup view is correctly expanded when inspecting an element nested
// in several shadow roots:
// - when using the context-menu "Inspect element"
// - when using the element picker

const TEST_URL = `data:text/html;charset=utf-8,` + encodeURIComponent(`
  <test-outer></test-outer>
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

    defineComponent('test-outer', \`
      <test-inner>
        <test-image></test-image>
      </test-inner>\`);

    defineComponent('test-inner', \`
      <div>
        <div>
          <div>
            <slot></slot>
          </div>
        </div>
      </div>\`);

    defineComponent('test-image',
      \`<div style="display:block; height: 200px; width: 100%; background:red"></div>\`);
  })();
  </script>`);

add_task(async function() {
  await enableWebComponents();

  const { inspector, toolbox, tab, testActor } = await openInspectorForURL(TEST_URL);

  info("Waiting for element picker to become active");
  await startPicker(toolbox);
  info("Click and pick the pick-target");
  await pickElement(inspector, testActor, "test-outer", 10, 10);
  info("Check that the markup view is displayed as expected");
  await assertMarkupView(inspector);

  info("Close DevTools before testing Inspect Element");
  await gDevTools.closeToolbox(inspector.target);

  info("Waiting for element picker to become active.");
  const newTestActor = await getTestActorWithoutToolbox(tab);
  info("Click on Inspect Element for our test-image <div>");
  // Note: we click on test-outer, because we can't find the <div> using a simple
  // querySelector. However the click is simulated in the middle of the <test-outer>
  // component, and will always hit the test <div> which takes all the space.
  const newInspector = await clickOnInspectMenuItem(newTestActor, "test-outer");
  info("Check again that the markup view is displayed as expected");
  await assertMarkupView(newInspector);
});

async function assertMarkupView(inspector) {
  const outerFront = await getNodeFront("test-outer", inspector);
  const outerContainer = inspector.markup.getContainer(outerFront);
  assertContainer(outerContainer, {expanded: true, text: "test-outer", children: 1});

  const outerShadowContainer = outerContainer.getChildContainers()[0];
  assertContainer(outerShadowContainer,
    {expanded: true, text: "#shadow-root", children: 1});

  const innerContainer = outerShadowContainer.getChildContainers()[0];
  assertContainer(innerContainer, {expanded: true, text: "test-inner", children: 2});

  const innerShadowContainer = innerContainer.getChildContainers()[0];
  const imageContainer = innerContainer.getChildContainers()[1];
  assertContainer(innerShadowContainer, {expanded: false, text: "#shadow-root"});
  assertContainer(imageContainer, {expanded: true, text: "test-image", children: 1});

  const imageShadowContainer = imageContainer.getChildContainers()[0];
  assertContainer(imageShadowContainer,
    {expanded: true, text: "#shadow-root", children: 1});

  const redDivContainer = imageShadowContainer.getChildContainers()[0];
  assertContainer(redDivContainer, {expanded: false, text: "div"});
  is(redDivContainer.selected, true, "Div element is selected as expected");
}

/**
 * Check if the provided markup container is expanded, has the expected text and the
 * expected number of children.
 */
function assertContainer(container, {expanded, text, children}) {
  is(container.expanded, expanded, "Container is expanded");
  assertContainerHasText(container, text);
  if (expanded) {
    const childContainers = container.getChildContainers();
    is(childContainers.length, children, "Container has expected number of children");
  }
}
