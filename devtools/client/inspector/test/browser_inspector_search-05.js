/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Testing that when search results contain suggestions for nodes in other
// frames, selecting these suggestions actually selects the right nodes.

requestLongerTimeout(2);

add_task(async function() {
  const { inspector } = await openInspectorForURL(
    `${URL_ROOT_ORG}doc_inspector_search-iframes.html`
  );

  info("Focus the search box");
  await focusSearchBoxUsingShortcut(inspector.panelWin);

  info("Enter # to search for all ids");
  let processingDone = once(inspector.searchSuggestions, "processing-done");
  EventUtils.synthesizeKey("#", {}, inspector.panelWin);
  await processingDone;

  info("Wait for search query to complete");
  await inspector.searchSuggestions._lastQuery;

  info("Press tab to fill the search input with the first suggestion");
  processingDone = once(inspector.searchSuggestions, "processing-done");
  EventUtils.synthesizeKey("VK_TAB", {}, inspector.panelWin);
  await processingDone;

  info("Press enter and expect a new selection");
  let onSelect = inspector.once("inspector-updated");
  EventUtils.synthesizeKey("VK_RETURN", {}, inspector.panelWin);
  await onSelect;

  await checkCorrectButton(inspector, "#iframe-1");

  info("Press enter to cycle through multiple nodes matching this suggestion");
  onSelect = inspector.once("inspector-updated");
  EventUtils.synthesizeKey("VK_RETURN", {}, inspector.panelWin);
  await onSelect;

  await checkCorrectButton(inspector, "#iframe-2");

  info("Press enter to cycle through multiple nodes matching this suggestion");
  onSelect = inspector.once("inspector-updated");
  EventUtils.synthesizeKey("VK_RETURN", {}, inspector.panelWin);
  await onSelect;

  await checkCorrectButton(inspector, "#iframe-3");

  info("Press enter to cycle through multiple nodes matching this suggestion");
  onSelect = inspector.once("inspector-updated");
  EventUtils.synthesizeKey("VK_RETURN", {}, inspector.panelWin);
  await onSelect;

  await checkCorrectButton(inspector, "#iframe-4");

  info("Press enter to cycle through multiple nodes matching this suggestion");
  onSelect = inspector.once("inspector-updated");
  EventUtils.synthesizeKey("VK_RETURN", {}, inspector.panelWin);
  await onSelect;

  await checkCorrectButton(inspector, "#iframe-1");
});

const checkCorrectButton = async function(inspector, frameSelector) {
  const { walker } = inspector;

  const nodeFrontInfo = await getSelectedNodeFrontInfo(inspector);

  is(nodeFrontInfo.nodeFront.id, "b1", "The selected node is #b1");
  is(
    nodeFrontInfo.nodeFront.tagName.toLowerCase(),
    "button",
    "The selected node is <button>"
  );

  const iframes = await walker.multiFrameQuerySelectorAll(frameSelector);
  const iframe = await iframes.item(0);
  const expectedDocument = (await walker.children(iframe)).nodes[0];

  is(
    nodeFrontInfo.document,
    expectedDocument,
    "The selected node is in " + frameSelector
  );
};
/**
 * Gets the currently selected nodefront. It also finds the
 * document node which contains the node.
 * @param   {Object} inspector
 * @returns {Object}
 *          nodeFront - The currently selected nodeFront
 *          document - The document which contains the node.
 *
 */
async function getSelectedNodeFrontInfo(inspector) {
  const { selection, commands } = inspector;

  const nodeFront = selection.nodeFront;
  const inspectors = await commands.inspectorCommand.getAllInspectorFronts();

  for (let i = 0; i < inspectors.length; i++) {
    const inspectorFront = inspectors[i];
    if (inspectorFront.walker == nodeFront.walkerFront) {
      const document = await inspectorFront.walker.document(nodeFront);
      return { nodeFront, document };
    }
  }
  return null;
}
