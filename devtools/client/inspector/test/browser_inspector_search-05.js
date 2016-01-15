/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Testing that when search results contain suggestions for nodes in other
// frames, selecting these suggestions actually selects the right nodes.

const IFRAME_SRC = "doc_inspector_search.html";
const TEST_URL = "data:text/html;charset=utf-8," +
                 "<iframe id=\"iframe-1\" src=\"" +
                 URL_ROOT + IFRAME_SRC + "\"></iframe>" +
                 "<iframe id=\"iframe-2\" src=\"" +
                 URL_ROOT + IFRAME_SRC + "\"></iframe>" +
                 "<iframe id='iframe-3' src='data:text/html," +
                   "<button id=\"b1\">Nested button</button>" +
                   "<iframe id=\"iframe-4\" src=" + URL_ROOT + IFRAME_SRC + "></iframe>'>" +
                 "</iframe>";

add_task(function* () {
  let {inspector} = yield openInspectorForURL(TEST_URL);

  let searchBox = inspector.searchBox;
  let popup = inspector.searchSuggestions.searchPopup;

  info("Focus the search box");
  yield focusSearchBoxUsingShortcut(inspector.panelWin);

  info("Enter # to search for all ids");
  let processingDone = once(inspector.searchSuggestions, "processing-done");
  EventUtils.synthesizeKey("#", {}, inspector.panelWin);
  yield processingDone;

  info("Wait for search query to complete");
  yield inspector.searchSuggestions._lastQuery;

  info("Press tab to fill the search input with the first suggestion");
  processingDone = once(inspector.searchSuggestions, "processing-done");
  EventUtils.synthesizeKey("VK_TAB", {}, inspector.panelWin);
  yield processingDone;

  info("Press enter and expect a new selection");
  let onSelect = inspector.once("inspector-updated");
  EventUtils.synthesizeKey("VK_RETURN", {}, inspector.panelWin);
  yield onSelect;

  yield checkCorrectButton(inspector, "#iframe-1");

  info("Press enter to cycle through multiple nodes matching this suggestion");
  onSelect = inspector.once("inspector-updated");
  EventUtils.synthesizeKey("VK_RETURN", {}, inspector.panelWin);
  yield onSelect;

  yield checkCorrectButton(inspector, "#iframe-2");

  info("Press enter to cycle through multiple nodes matching this suggestion");
  onSelect = inspector.once("inspector-updated");
  EventUtils.synthesizeKey("VK_RETURN", {}, inspector.panelWin);
  yield onSelect;

  yield checkCorrectButton(inspector, "#iframe-3");

  info("Press enter to cycle through multiple nodes matching this suggestion");
  onSelect = inspector.once("inspector-updated");
  EventUtils.synthesizeKey("VK_RETURN", {}, inspector.panelWin);
  yield onSelect;

  yield checkCorrectButton(inspector, "#iframe-4");

  info("Press enter to cycle through multiple nodes matching this suggestion");
  onSelect = inspector.once("inspector-updated");
  EventUtils.synthesizeKey("VK_RETURN", {}, inspector.panelWin);
  yield onSelect;

  yield checkCorrectButton(inspector, "#iframe-1");
});

let checkCorrectButton = Task.async(function*(inspector, frameSelector) {
  let {walker} = inspector;
  let node = inspector.selection.nodeFront;

  ok(node.id, "b1", "The selected node is #b1");
  ok(node.tagName.toLowerCase(), "button",
    "The selected node is <button>");

  let selectedNodeDoc = yield walker.document(node);
  let iframe = yield walker.multiFrameQuerySelectorAll(frameSelector);
  iframe = yield iframe.item(0);
  let iframeDoc = (yield walker.children(iframe)).nodes[0];
  is(selectedNodeDoc, iframeDoc, "The selected node is in " + frameSelector);
});
