/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Checks for the AccessibleWalkerActor

add_task(async function() {
  const {client, walker, accessibility} =
    await initAccessibilityFrontForUrl(MAIN_DOMAIN + "doc_accessibility.html");

  const a11yWalker = await accessibility.getWalker();
  ok(a11yWalker, "The AccessibleWalkerFront was returned");

  await accessibility.enable();
  const rootNode = await walker.getRootNode();
  const a11yDoc = await a11yWalker.getAccessibleFor(rootNode);
  ok(a11yDoc, "The AccessibleFront for root doc is created");

  const children = await a11yWalker.children();
  is(children.length, 1,
    "AccessibleWalker only has 1 child - root doc accessible");
  is(a11yDoc, children[0],
    "Root accessible must be AccessibleWalker's only child");

  const buttonNode = await walker.querySelector(walker.rootNode, "#button");
  const accessibleFront = await a11yWalker.getAccessibleFor(buttonNode);

  checkA11yFront(accessibleFront, {
    name: "Accessible Button",
    role: "pushbutton"
  });

  const ancestry = await a11yWalker.getAncestry(accessibleFront);
  is(ancestry.length, 1, "Button is a direct child of a root document.");
  is(ancestry[0].accessible, a11yDoc,
    "Button's only ancestor is a root document");
  is(ancestry[0].children.length, 3,
    "Root doc should have correct number of children");
  ok(ancestry[0].children.includes(accessibleFront),
    "Button accessible front is in root doc's children");

  const browser = gBrowser.selectedBrowser;

  // Ensure name-change event is emitted by walker when cached accessible's name
  // gets updated (via DOM manipularion).
  await emitA11yEvent(a11yWalker, "name-change",
    (front, parent) => {
      checkA11yFront(front, { name: "Renamed" }, accessibleFront);
      checkA11yFront(parent, { }, a11yDoc);
    },
    () => ContentTask.spawn(browser, null, () =>
      content.document.getElementById("button").setAttribute(
      "aria-label", "Renamed")));

  // Ensure reorder event is emitted by walker when DOM tree changes.
  let docChildren = await a11yDoc.children();
  is(docChildren.length, 3, "Root doc should have correct number of children");

  await emitA11yEvent(a11yWalker, "reorder",
    front => checkA11yFront(front, { }, a11yDoc),
    () => ContentTask.spawn(browser, null, () => {
      const input = content.document.createElement("input");
      input.type = "text";
      input.title = "This is a tooltip";
      input.value = "New input";
      content.document.body.appendChild(input);
    }));

  docChildren = await a11yDoc.children();
  is(docChildren.length, 4, "Root doc should have correct number of children");

  // Ensure destory event is emitted by walker when cached accessible's raw
  // accessible gets destroyed.
  await emitA11yEvent(a11yWalker, "accessible-destroy",
    destroyedFront => checkA11yFront(destroyedFront, { }, accessibleFront),
    () => ContentTask.spawn(browser, null, () =>
      content.document.getElementById("button").remove()));

  let shown = await a11yWalker.highlightAccessible(docChildren[0]);
  ok(shown, "AccessibleHighlighter highlighted the node");

  shown = await a11yWalker.highlightAccessible(a11yDoc);
  ok(!shown, "AccessibleHighlighter does not highlight an accessible with no bounds");
  await a11yWalker.unhighlight();

  info("Checking AccessibleWalker picker functionality");
  ok(a11yWalker.pick, "AccessibleWalker pick method exists");
  ok(a11yWalker.pickAndFocus, "AccessibleWalker pickAndFocus method exists");
  ok(a11yWalker.cancelPick, "AccessibleWalker cancelPick method exists");

  let onPickerEvent = a11yWalker.once("picker-accessible-hovered");
  await a11yWalker.pick();
  await BrowserTestUtils.synthesizeMouseAtCenter("#h1", { type: "mousemove" }, browser);
  let acc = await onPickerEvent;
  checkA11yFront(acc, { name: "Accessibility Test" }, docChildren[0]);

  onPickerEvent = a11yWalker.once("picker-accessible-previewed");
  await BrowserTestUtils.synthesizeMouseAtCenter("#h1", { shiftKey: true }, browser);
  acc = await onPickerEvent;
  checkA11yFront(acc, { name: "Accessibility Test" }, docChildren[0]);

  onPickerEvent = a11yWalker.once("picker-accessible-canceled");
  await BrowserTestUtils.synthesizeKey("VK_ESCAPE", { type: "keydown" }, browser);
  await onPickerEvent;

  onPickerEvent = a11yWalker.once("picker-accessible-hovered");
  await a11yWalker.pick();
  await BrowserTestUtils.synthesizeMouseAtCenter("#h1", { type: "mousemove" }, browser);
  await onPickerEvent;

  onPickerEvent = a11yWalker.once("picker-accessible-picked");
  await BrowserTestUtils.synthesizeMouseAtCenter("#h1", { }, browser);
  acc = await onPickerEvent;
  checkA11yFront(acc, { name: "Accessibility Test" }, docChildren[0]);

  await a11yWalker.cancelPick();

  info("Checking document-ready event fired by walker when top level accessible " +
       "document is recreated.");
  const reloaded = BrowserTestUtils.browserLoaded(browser);
  const documentReady = a11yWalker.once("document-ready");
  browser.reload();
  await reloaded;
  await documentReady;

  await accessibility.disable();
  await waitForA11yShutdown();
  await client.close();
  gBrowser.removeCurrentTab();
});
