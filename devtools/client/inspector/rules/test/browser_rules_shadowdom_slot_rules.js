/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that when selecting a slot element, the rule view displays the rules for the
// corresponding element.

const TEST_URL = `data:text/html;charset=utf-8,` + encodeURIComponent(`
  <html>
  <head>
  <style>
    #el1 { color: red }
    #el2 { color: blue }
  </style>
  </head>
  <body>
  <test-component>
    <div slot="slot1" id="el1">slot1-1</div>
    <div slot="slot1" id="el2">slot1-2</div>
  </test-component>

  <script>
    'use strict';
    customElements.define('test-component', class extends HTMLElement {
      constructor() {
        super();
        let shadowRoot = this.attachShadow({mode: 'open'});
        shadowRoot.innerHTML = '<slot name="slot1"></slot>';
      }
    });
  </script>
  </body>
  </html>
`);

add_task(async function() {
  await pushPref("dom.webcomponents.shadowdom.enabled", true);
  await pushPref("dom.webcomponents.customelements.enabled", true);

  const {inspector} = await openInspectorForURL(TEST_URL);
  const {markup} = inspector;
  const ruleview = inspector.getPanel("ruleview").view;

  // <test-component> is a shadow host.
  info("Find and expand the test-component shadow DOM host.");
  const hostFront = await getNodeFront("test-component", inspector);

  await markup.expandNode(hostFront);
  await waitForMultipleChildrenUpdates(inspector);

  info("Test that expanding a shadow host shows shadow root and one host child.");
  const hostContainer = markup.getContainer(hostFront);

  info("Expand the shadow root");
  const childContainers = hostContainer.getChildContainers();
  const shadowRootContainer = childContainers[0];
  await expandContainer(inspector, shadowRootContainer);

  info("Expand the slot");
  const shadowChildContainers = shadowRootContainer.getChildContainers();
  const slotContainer = shadowChildContainers[0];
  await expandContainer(inspector, slotContainer);

  const slotChildContainers = slotContainer.getChildContainers();
  is(slotChildContainers.length, 2, "Expecting 2 slotted children");

  info("Select slotted node and check that the rule view displays correct content");
  await selectNode(slotChildContainers[0].node, inspector);
  checkRule(ruleview, "#el1", "color", "red");

  info("Select another slotted node and check the rule view");
  await selectNode(slotChildContainers[1].node, inspector);
  checkRule(ruleview, "#el2", "color", "blue");
});

function checkRule(ruleview, selector, name, expectedValue) {
  const rule = getRuleViewRule(ruleview, selector);
  ok(rule, "ruleview shows the expected rule for slotted " + selector);
  const value = getRuleViewPropertyValue(ruleview, selector, name);
  is(value, expectedValue, "ruleview shows the expected value for slotted " + selector);
}
