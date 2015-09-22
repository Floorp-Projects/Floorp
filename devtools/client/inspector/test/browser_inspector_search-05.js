/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Testing that when search results contain suggestions for nodes in other
// frames, selecting these suggestions actually selects the right nodes.

const IFRAME_SRC = "doc_inspector_search.html";
const TEST_URL = "data:text/html;charset=utf-8," +
                 "<iframe id=\"iframe-1\" src=\"" +
                 TEST_URL_ROOT + IFRAME_SRC + "\"></iframe>" +
                 "<iframe id=\"iframe-2\" src=\"" +
                 TEST_URL_ROOT + IFRAME_SRC + "\"></iframe>";

add_task(function* () {
  let {inspector} = yield openInspectorForURL(TEST_URL);
  let {walker} = inspector;

  let searchBox = inspector.searchBox;
  let popup = inspector.searchSuggestions.searchPopup;

  info("Focus the search box");
  yield focusSearchBoxUsingShortcut(inspector.panelWin);

  info("Enter # to search for all ids");
  let command = once(searchBox, "command");
  EventUtils.synthesizeKey("#", {}, inspector.panelWin);
  yield command;

  info("Wait for search query to complete");
  yield inspector.searchSuggestions._lastQuery;

  info("Press tab to fill the search input with the first suggestion and " +
    "expect a new selection");
  let onSelect = inspector.once("inspector-updated");
  EventUtils.synthesizeKey("VK_TAB", {}, inspector.panelWin);
  yield onSelect;

  let node = inspector.selection.nodeFront;
  ok(node.id, "b1", "The selected node is #b1");
  ok(node.tagName.toLowerCase(), "button",
    "The selected node is <button>");

  let selectedNodeDoc = yield walker.document(node);
  let iframe1 = yield walker.querySelector(walker.rootNode, "#iframe-1");
  let iframe1Doc = (yield walker.children(iframe1)).nodes[0];
  is(selectedNodeDoc, iframe1Doc, "The selected node is in iframe 1");

  info("Press enter to cycle through multiple nodes matching this suggestion");
  onSelect = inspector.once("inspector-updated");
  EventUtils.synthesizeKey("VK_RETURN", {}, inspector.panelWin);
  yield onSelect;

  node = inspector.selection.nodeFront;
  ok(node.id, "b1", "The selected node is #b1 again");
  ok(node.tagName.toLowerCase(), "button",
    "The selected node is <button> again");

  selectedNodeDoc = yield walker.document(node);
  let iframe2 = yield walker.querySelector(walker.rootNode, "#iframe-2");
  let iframe2Doc = (yield walker.children(iframe2)).nodes[0];
  is(selectedNodeDoc, iframe2Doc, "The selected node is in iframe 2");
});
