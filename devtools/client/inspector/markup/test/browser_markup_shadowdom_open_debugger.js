/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that markup view displays a "custom" badge for custom elements.
// Test that the context menu also has a menu item to show the custom element definition.
// Test that clicking on any of those opens the debugger.
// Test that the markup view is correctly updated to show those items if the custom
// element definition happens after opening the inspector.

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/debugger/test/mochitest/shared-head.js",
  this
);

const TEST_URL =
  `data:text/html;charset=utf-8,` +
  encodeURIComponent(`
<test-component></test-component>
<other-component>some-content</other-component>

<script>
  "use strict";
  window.attachTestComponent = function() {
    customElements.define("test-component", class extends HTMLElement {
      constructor() {
        super();
        let shadowRoot = this.attachShadow({mode: "open"});
        shadowRoot.innerHTML = "<slot>some default content</slot>";
      }
      connectedCallback() {}
      disconnectedCallback() {}
    });
  }

  window.defineOtherComponent = function() {
    customElements.define('other-component', class extends HTMLParagraphElement {
      constructor() {
        super();
      }
    }, { extends: 'p' });
  }
</script>`);

add_task(async function () {
  const { inspector, toolbox } = await openInspectorForURL(TEST_URL);

  // Test with an element to which we attach a shadow.
  await runTest(inspector, toolbox, "test-component", "attachTestComponent");

  // Test with an element to which we only add a custom element definition.
  await runTest(inspector, toolbox, "other-component", "defineOtherComponent");
});

async function runTest(inspector, toolbox, selector, contentMethod) {
  // Test element is a regular element (no shadow root or custom element definition).
  info(`Select <${selector}>.`);
  await selectNode(selector, inspector);
  const testFront = await getNodeFront(selector, inspector);
  const testContainer = inspector.markup.getContainer(testFront);
  let customBadge = testContainer.elt.querySelector(
    ".inspector-badge.interactive[data-custom]"
  );

  // Verify that the "custom" badge and menu item are hidden.
  ok(!customBadge, "[custom] badge is hidden");
  let menuItem = getMenuItem("node-menu-jumptodefinition", inspector);
  ok(
    !menuItem,
    selector + ": The menu item was not found in the contextual menu"
  );

  info(
    "Call the content method that should attach a custom element definition"
  );
  const mutated = waitForMutation(inspector, "customElementDefined");
  SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [{ contentMethod }],
    function (args) {
      content.wrappedJSObject[args.contentMethod]();
    }
  );
  await mutated;

  // Test element should now have a custom element definition.

  // Check that the badge opens the debugger.
  customBadge = testContainer.elt.querySelector(
    ".inspector-badge.interactive[data-custom]"
  );
  ok(customBadge, "[custom] badge is visible");
  ok(
    !customBadge.hasAttribute("aria-pressed"),
    "[custom] badge is not a toggle button"
  );

  info("Click on the `custom` badge and verify that the debugger opens.");
  let onDebuggerReady = toolbox.getPanelWhenReady("jsdebugger");
  customBadge.click();
  await onDebuggerReady;

  const debuggerContext = createDebuggerContext(toolbox);
  await waitUntilDebuggerReady(debuggerContext);
  ok(true, "The debugger was opened when clicking on the custom badge");

  info("Switch to the inspector");
  await toolbox.selectTool("inspector");

  // Check that the debugger can be opened with the keyboard.
  info("Press the Enter key and verify that the debugger opens.");
  customBadge.focus();
  onDebuggerReady = toolbox.getPanelWhenReady("jsdebugger");
  EventUtils.synthesizeKey("VK_RETURN", {}, customBadge.ownerGlobal);

  await onDebuggerReady;
  await waitUntilDebuggerReady(debuggerContext);
  ok(true, "The debugger was opened via the keyboard");

  info("Switch to the inspector");
  await toolbox.selectTool("inspector");

  // Check that the menu item also opens the debugger.
  menuItem = getMenuItem("node-menu-jumptodefinition", inspector);
  ok(menuItem, selector + ": The menu item was found in the contextual menu");
  ok(!menuItem.disabled, selector + ": The menu item is not disabled");

  info("Click on `Jump to Definition` and verify that the debugger opens.");
  onDebuggerReady = toolbox.getPanelWhenReady("jsdebugger");
  menuItem.click();
  await onDebuggerReady;

  await waitUntilDebuggerReady(debuggerContext);
  ok(true, "The debugger was opened via the context menu");

  info("Switch to the inspector");
  await toolbox.selectTool("inspector");
}

function getMenuItem(id, inspector) {
  const allMenuItems = openContextMenuAndGetAllItems(inspector);
  return allMenuItems.find(i => i.id === "node-menu-jumptodefinition");
}

async function waitUntilDebuggerReady(debuggerContext) {
  info("Wait until source is loaded in the debugger");

  // We have to wait until the debugger has fully loaded the source otherwise
  // we will get unhandled promise rejections.
  await waitForLoadedSource(debuggerContext, TEST_URL);
}
