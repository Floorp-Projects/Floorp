/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that cycling focus with keyboard (via TAB key) in slotted nodes works.

const TEST_URL = `data:text/html;charset=utf-8,
  <test-component>
    <div slot="slot1" id="el1">slot1-1</div>
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
  </script>`;

add_task(async function() {
  const { inspector } = await openInspectorForURL(TEST_URL);
  const { markup } = inspector;
  const win = inspector.markup.doc.defaultView;

  info("Find and expand the test-component shadow DOM host.");
  const hostFront = await getNodeFront("test-component", inspector);
  const hostContainer = markup.getContainer(hostFront);
  await expandContainer(inspector, hostContainer);

  info("Expand the shadow root");
  const shadowRootContainer = hostContainer.getChildContainers()[0];
  await expandContainer(inspector, shadowRootContainer);

  info("Expand the slot");
  const slotContainer = shadowRootContainer.getChildContainers()[0];
  await expandContainer(inspector, slotContainer);

  info("Select the slotted container for the element");
  const node = slotContainer.getChildContainers()[0].node;
  const container = inspector.markup.getContainer(node, true);
  await selectNode(node, inspector, "no-reason", true);

  const root = inspector.markup.getContainer(inspector.markup._rootNode);
  root.elt.focus();
  const tagSpan = container.elt.querySelector(".tag");
  const revealLink = container.elt.querySelector(".reveal-link");

  info("Hit Enter to focus on the first element");
  let tagFocused = once(tagSpan, "focus");
  EventUtils.synthesizeAndWaitKey("KEY_Enter", {}, win);
  await tagFocused;

  info("Hit Tab to focus on the next element");
  const linkFocused = once(revealLink, "focus");
  EventUtils.synthesizeKey("KEY_Tab", {}, win);
  await linkFocused;

  info("Hit Tab again to cycle focus to the first element");
  tagFocused = once(tagSpan, "focus");
  EventUtils.synthesizeKey("KEY_Tab", {}, win);
  await tagFocused;

  ok(
    inspector.markup.doc.activeElement === tagSpan,
    "Focus has gone back to first element"
  );
});
