/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that hovering over a simple selector in the style-editor requests the
// highlighting of the corresponding nodes, even in remote iframes.

const REMOTE_IFRAME_URL = `https://example.org/document-builder.sjs?html=
    <style>h2{color:cyan}</style>
    <h2>highlighter test</h2>`;
const TOP_LEVEL_URL = `https://example.com/document-builder.sjs?html=
    <style>h1{color:red}</style>
    <h1>highlighter test</h1>
    <iframe src='${REMOTE_IFRAME_URL}'></iframe>`;

add_task(async function() {
  const { ui } = await openStyleEditorForURL(TOP_LEVEL_URL);

  info(
    "Wait until both stylesheet are loaded and ready to handle mouse events"
  );
  await waitFor(() => ui.editors.length == 2);
  const topLevelStylesheetEditor = ui.editors.find(e =>
    e._resource.nodeHref.startsWith("https://example.com")
  );
  const iframeStylesheetEditor = ui.editors.find(e =>
    e._resource.nodeHref.startsWith("https://example.org")
  );

  await ui.selectStyleSheet(topLevelStylesheetEditor.styleSheet);
  await waitFor(() => topLevelStylesheetEditor.highlighter);

  info("Check that highlighting works on the top-level document");
  const topLevelHighlighterTestFront = await topLevelStylesheetEditor._resource.targetFront.getFront(
    "highlighterTest"
  );
  topLevelHighlighterTestFront.highlighter =
    topLevelStylesheetEditor.highlighter;

  info("Expecting a node-highlighted event");
  let onHighlighted = topLevelStylesheetEditor.once("node-highlighted");

  info("Simulate a mousemove event on the h1 selector");
  // mousemove event listeners is set on editor.sourceEditor, which is not defined right away.
  await waitFor(() => !!topLevelStylesheetEditor.sourceEditor);
  let selectorEl = querySelectorCodeMirrorCssRuleSelectorToken(
    topLevelStylesheetEditor
  );
  EventUtils.synthesizeMouseAtCenter(
    selectorEl,
    { type: "mousemove" },
    selectorEl.ownerDocument.defaultView
  );
  await onHighlighted;

  ok(
    await topLevelHighlighterTestFront.isNodeRectHighlighted(
      await getElementNodeRectWithinTarget(["h1"])
    ),
    "The highlighter's outline corresponds to the h1 node"
  );

  info(
    "Simulate a mousemove event on the property name to hide the highlighter"
  );
  EventUtils.synthesizeMouseAtCenter(
    querySelectorCodeMirrorCssPropertyNameToken(topLevelStylesheetEditor),
    { type: "mousemove" },
    selectorEl.ownerDocument.defaultView
  );

  await waitFor(async () => !topLevelStylesheetEditor.highlighter.isShown());
  let isVisible = await topLevelHighlighterTestFront.isHighlighting();
  is(isVisible, false, "The highlighter is now hidden");

  info("Check that highlighting works on the iframe document");
  await ui.selectStyleSheet(iframeStylesheetEditor.styleSheet);
  await waitFor(() => iframeStylesheetEditor.highlighter);

  const iframeHighlighterTestFront = await iframeStylesheetEditor._resource.targetFront.getFront(
    "highlighterTest"
  );
  iframeHighlighterTestFront.highlighter = iframeStylesheetEditor.highlighter;

  info("Expecting a node-highlighted event");
  onHighlighted = iframeStylesheetEditor.once("node-highlighted");

  info("Simulate a mousemove event on the h2 selector");
  // mousemove event listeners is set on editor.sourceEditor, which is not defined right away.
  await waitFor(() => !!iframeStylesheetEditor.sourceEditor);
  selectorEl = querySelectorCodeMirrorCssRuleSelectorToken(
    iframeStylesheetEditor
  );
  EventUtils.synthesizeMouseAtCenter(
    selectorEl,
    { type: "mousemove" },
    selectorEl.ownerDocument.defaultView
  );
  await onHighlighted;

  isVisible = await iframeHighlighterTestFront.isHighlighting();
  ok(isVisible, "The highlighter is shown");
  ok(
    await iframeHighlighterTestFront.isNodeRectHighlighted(
      await getElementNodeRectWithinTarget(["iframe", "h2"])
    ),
    "The highlighter's outline corresponds to the h2 node"
  );

  info("Simulate a mousemove event elsewhere in the editor");
  EventUtils.synthesizeMouseAtCenter(
    querySelectorCodeMirrorCssPropertyNameToken(iframeStylesheetEditor),
    { type: "mousemove" },
    selectorEl.ownerDocument.defaultView
  );

  await waitFor(async () => !topLevelStylesheetEditor.highlighter.isShown());

  isVisible = await iframeHighlighterTestFront.isHighlighting();
  is(isVisible, false, "The highlighter is now hidden");
});

function querySelectorCodeMirrorCssRuleSelectorToken(stylesheetEditor) {
  // CSS Rules selector (e.g. `h1`) are displayed in a .cm-tag span
  return querySelectorCodeMirror(stylesheetEditor, ".cm-tag");
}

function querySelectorCodeMirrorCssPropertyNameToken(stylesheetEditor) {
  // properties name (e.g. `color`) are displayed in a .cm-property span
  return querySelectorCodeMirror(stylesheetEditor, ".cm-property");
}

function querySelectorCodeMirror(stylesheetEditor, selector) {
  return stylesheetEditor.sourceEditor.codeMirror
    .getWrapperElement()
    .querySelector(selector);
}

/**
 * Return the bounds of the element matching the selector, relatively to the target bounds
 * (e.g. if Fission is enabled, it's related to the iframe bound, if Fission is disabled,
 * it's related to the top level document).
 *
 * @param {Array<string>} selectors: Arrays of CSS selectors from the root document to the node.
 *        The last CSS selector of the array is for the node in its frame doc.
 *        The before-last CSS selector is for the frame in its parent frame, etc...
 *        Ex: ["frame.first-frame", ..., "frame.last-frame", ".target-node"]
 * @returns {Object} with left/top/width/height properties representing the node bounds
 */
async function getElementNodeRectWithinTarget(selectors) {
  // Retrieve the browsing context in which the element is
  const inBCSelector = selectors.pop();
  const frameSelectors = selectors;
  const bc = frameSelectors.length
    ? await getBrowsingContextInFrames(
        gBrowser.selectedBrowser.browsingContext,
        frameSelectors
      )
    : gBrowser.selectedBrowser.browsingContext;

  // Get the element bounds within the Firefox window
  const elementBounds = await SpecialPowers.spawn(
    bc,
    [inBCSelector],
    _selector => {
      const el = content.document.querySelector(_selector);
      const {
        left,
        top,
        width,
        height,
      } = el.getBoxQuadsFromWindowOrigin()[0].getBounds();
      return { left, top, width, height };
    }
  );

  // Then we need to offset the element bounds from a frame bounds
  // When fission/EFT is enabled, the highlighter is only shown within the iframe bounds.
  // So we only need to retrieve the element bounds within the iframe.
  // Otherwise, we retrieve the top frame bounds
  const relativeBrowsingContext =
    isFissionEnabled() || isEveryFrameTargetEnabled()
      ? bc
      : gBrowser.selectedBrowser.browsingContext;
  const relativeDocumentBounds = await SpecialPowers.spawn(
    relativeBrowsingContext,
    [],
    () =>
      content.document.documentElement
        .getBoxQuadsFromWindowOrigin()[0]
        .getBounds()
  );

  // Adjust the element bounds based on the relative document bounds
  elementBounds.left = elementBounds.left - relativeDocumentBounds.left;
  elementBounds.top = elementBounds.top - relativeDocumentBounds.top;

  return elementBounds;
}
