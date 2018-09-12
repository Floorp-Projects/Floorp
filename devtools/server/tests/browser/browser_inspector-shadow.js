"use strict";

const URL = MAIN_DOMAIN + "inspector-shadow.html";

add_task(async function() {
  info("Test that a shadow host has a shadow root");
  const { walker } = await initInspectorFront(URL);

  const el = await walker.querySelector(walker.rootNode, "#empty");
  const children = await walker.children(el);

  is(el.displayName, "test-empty", "#empty exists");
  ok(el.isShadowHost, "#empty is a shadow host");

  const shadowRoot = children.nodes[0];
  ok(shadowRoot.isShadowRoot, "#empty has a shadow-root child");
  is(children.nodes.length, 1, "#empty has no other children");
});

add_task(async function() {
  info("Test that a shadow host has its children too");
  const { walker } = await initInspectorFront(URL);

  const el = await walker.querySelector(walker.rootNode, "#one-child");
  const children = await walker.children(el);

  is(children.nodes.length, 2, "#one-child has two children " +
    "(shadow root + another child)");
  ok(children.nodes[0].isShadowRoot, "First child is a shadow-root");
  is(children.nodes[1].displayName, "h1", "Second child is <h1>");
});

add_task(async function() {
  info("Test that shadow-root has its children");
  const { walker } = await initInspectorFront(URL);

  const el = await walker.querySelector(walker.rootNode, "#shadow-children");
  ok(el.isShadowHost, "#shadow-children is a shadow host");

  const children = await walker.children(el);
  ok(children.nodes.length === 1 && children.nodes[0].isShadowRoot,
    "#shadow-children has only one child and it's a shadow-root");

  const shadowRoot = children.nodes[0];
  const shadowChildren = await walker.children(shadowRoot);
  is(shadowChildren.nodes.length, 2, "shadow-root has two children");
  is(shadowChildren.nodes[0].displayName, "h1", "First child is <h1>");
  is(shadowChildren.nodes[1].displayName, "p", "Second child is <p>");
});

add_task(async function() {
  info("Test that shadow root has its children and slotted nodes");
  const { walker } = await initInspectorFront(URL);

  const el = await walker.querySelector(walker.rootNode, "#named-slot");
  ok(el.isShadowHost, "#named-slot is a shadow host");

  const children = await walker.children(el);
  is(children.nodes.length, 2, "#named-slot has two children");
  const shadowRoot = children.nodes[0];
  ok(shadowRoot.isShadowRoot, "#named-slot has a shadow-root child");

  const slotted = children.nodes[1];
  is(slotted.getAttribute("slot"), "slot1", "#named-slot as a child that is slotted");

  const shadowChildren = await walker.children(shadowRoot);
  is(shadowChildren.nodes[0].displayName, "h1",
    "shadow-root first child is a regular <h1> tag");
  is(shadowChildren.nodes[1].displayName, "slot",
    "shadow-root second child is a slot");

  const slottedChildren = await walker.children(shadowChildren.nodes[1]);
  is(slottedChildren.nodes[0], slotted, "The slot has the slotted node as a child");
});

add_task(async function() {
  info("Test pseudoelements in shadow host");
  const { walker } = await initInspectorFront(URL);

  const el = await walker.querySelector(walker.rootNode, "#host-pseudo");
  const children = await walker.children(el);

  ok(children.nodes[0].isShadowRoot, "#host-pseudo 1st child is a shadow root");
  ok(children.nodes[1].isBeforePseudoElement, "#host-pseudo 2nd child is ::before");
  ok(children.nodes[2].isAfterPseudoElement, "#host-pseudo 3rd child is ::after");
});

add_task(async function() {
  info("Test pseudoelements in slotted nodes");
  const { walker } = await initInspectorFront(URL);

  const el = await walker.querySelector(walker.rootNode, "#slot-pseudo");
  const shadowRoot = (await walker.children(el)).nodes[0];
  ok(shadowRoot.isShadowRoot, "#slot-pseudo has a shadow-root child");

  const shadowChildren = await walker.children(shadowRoot);
  is(shadowChildren.nodes[1].displayName, "slot", "shadow-root has a slot");

  const slottedChildren = await walker.children(shadowChildren.nodes[1]);
  ok(slottedChildren.nodes[0].isBeforePseudoElement, "slot has ::before");
  ok(slottedChildren.nodes[2].isAfterPseudoElement, "slot has ::after");
});

add_task(async function() {
  info("Test open/closed modes in shadow roots");
  const { walker } = await initInspectorFront(URL);

  const openEl = await walker.querySelector(walker.rootNode, "#mode-open");
  const openShadowRoot = (await walker.children(openEl)).nodes[0];
  const closedEl = await walker.querySelector(walker.rootNode, "#mode-closed");
  const closedShadowRoot = (await walker.children(closedEl)).nodes[0];

  is(openShadowRoot.shadowRootMode, "open",
    "#mode-open has a shadow root with open mode");
  is(closedShadowRoot.shadowRootMode, "closed",
    "#mode-closed has a shadow root with closed mode");
});

add_task(async function() {
  info("Test that slotted inline text nodes appear in the Shadow DOM tree");
  const { walker } = await initInspectorFront(URL);

  const el = await walker.querySelector(walker.rootNode, "#slot-inline-text");
  const hostChildren = await walker.children(el);
  const originalSlot = hostChildren.nodes[1];
  is(originalSlot.displayName, "#text", "Shadow host as a text node to be slotted");

  const shadowRoot = hostChildren.nodes[0];
  const shadowChildren = await walker.children(shadowRoot);
  const slot = shadowChildren.nodes[0];
  is(slot.displayName, "slot", "shadow-root has a slot child");
  ok(!slot._form.inlineTextChild, "Slotted node is not an inline text");

  const slotChildren = await walker.children(slot);
  const slotted = slotChildren.nodes[0];
  is(slotted.displayName, "#text", "Slotted node is a text node");
  is(slotted._form.nodeValue, originalSlot._form.nodeValue,
    "Slotted content is the same as original's");
});
