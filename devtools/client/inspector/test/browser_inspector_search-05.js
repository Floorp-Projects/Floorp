/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Testing that when search results contain suggestions for nodes in other
// frames, selecting these suggestions actually selects the right nodes.

requestLongerTimeout(2);

const IFRAME_SRC = "doc_inspector_search.html";
const NESTED_IFRAME_SRC = `
  <button id="b1">Nested button</button>
  <iframe id="iframe-4" src="${URL_ROOT + IFRAME_SRC}"></iframe>
`;
const TEST_URL = `
  <iframe id="iframe-1" src="${URL_ROOT + IFRAME_SRC}"></iframe>
  <iframe id="iframe-2" src="${URL_ROOT + IFRAME_SRC}"></iframe>
  <iframe id="iframe-3"
          src="data:text/html;charset=utf-8,${encodeURI(NESTED_IFRAME_SRC)}">
  </iframe>
`;

add_task(async function() {
  const {inspector} = await openInspectorForURL(
    "data:text/html;charset=utf-8," + encodeURI(TEST_URL));

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
  const {walker} = inspector;
  const node = inspector.selection.nodeFront;

  ok(node.id, "b1", "The selected node is #b1");
  ok(node.tagName.toLowerCase(), "button",
    "The selected node is <button>");

  const selectedNodeDoc = await walker.document(node);
  let iframe = await walker.multiFrameQuerySelectorAll(frameSelector);
  iframe = await iframe.item(0);
  const iframeDoc = (await walker.children(iframe)).nodes[0];
  is(selectedNodeDoc, iframeDoc, "The selected node is in " + frameSelector);
};
