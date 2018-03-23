/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Testing navigation between nodes in search results
const {AppConstants} = require("resource://gre/modules/AppConstants.jsm");

const TEST_URL = URL_ROOT + "doc_inspector_search.html";

add_task(function* () {
  let {inspector} = yield openInspectorForURL(TEST_URL);

  info("Focus the search box");
  yield focusSearchBoxUsingShortcut(inspector.panelWin);

  info("Enter body > p to search");
  let processingDone = once(inspector.searchSuggestions, "processing-done");
  EventUtils.sendString("body > p", inspector.panelWin);
  yield processingDone;

  info("Wait for search query to complete");
  yield inspector.searchSuggestions._lastQuery;

  let msg = "Press enter and expect a new selection";
  yield sendKeyAndCheck(inspector, msg, "VK_RETURN", {}, "#p1");

  msg = "Press enter to cycle through multiple nodes";
  yield sendKeyAndCheck(inspector, msg, "VK_RETURN", {}, "#p2");

  msg = "Press shift-enter to select the previous node";
  yield sendKeyAndCheck(inspector, msg, "VK_RETURN", { shiftKey: true }, "#p1");

  if (AppConstants.platform === "macosx") {
    msg = "Press meta-g to cycle through multiple nodes";
    yield sendKeyAndCheck(inspector, msg, "VK_G", { metaKey: true }, "#p2");

    msg = "Press shift+meta-g to select the previous node";
    yield sendKeyAndCheck(inspector, msg, "VK_G",
                          { metaKey: true, shiftKey: true }, "#p1");
  } else {
    msg = "Press ctrl-g to cycle through multiple nodes";
    yield sendKeyAndCheck(inspector, msg, "VK_G", { ctrlKey: true }, "#p2");

    msg = "Press shift+ctrl-g to select the previous node";
    yield sendKeyAndCheck(inspector, msg, "VK_G",
                          { ctrlKey: true, shiftKey: true }, "#p1");
  }
});

let sendKeyAndCheck = Task.async(function* (inspector, description, key,
                                            modifiers, expectedId) {
  info(description);
  let onSelect = inspector.once("inspector-updated");
  EventUtils.synthesizeKey(key, modifiers, inspector.panelWin);
  yield onSelect;

  let selectedNode = inspector.selection.nodeFront;
  info(selectedNode.id + " is selected with text " + inspector.searchBox.value);
  let targetNode = yield getNodeFront(expectedId, inspector);
  is(selectedNode, targetNode, "Correct node " + expectedId + " is selected");
});
