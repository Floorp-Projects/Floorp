/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Tests for menuitem functionality that doesn't fit into any specific category
const TEST_URL = TEST_URL_ROOT + "doc_inspector_menu.html";
add_task(function* () {
  let { inspector, toolbox, testActor } = yield openInspectorForURL(TEST_URL);
  yield testShowDOMProperties();
  yield testDuplicateNode();
  yield testDeleteNode();
  yield testDeleteRootNode();
  yield testScrollIntoView();
  function* testShowDOMProperties() {
    info("Testing 'Show DOM Properties' menu item.");
    let showDOMPropertiesNode = inspector.panelDoc.getElementById("node-menu-showdomproperties");
    ok(showDOMPropertiesNode, "the popup menu has a show dom properties item");

    let consoleOpened = toolbox.once("webconsole-ready");

    info("Triggering 'Show DOM Properties' and waiting for inspector open");
    dispatchCommandEvent(showDOMPropertiesNode);
    yield consoleOpened;

    let webconsoleUI = toolbox.getPanel("webconsole").hud.ui;
    let messagesAdded = webconsoleUI.once("new-messages");
    yield messagesAdded;
    info("Checking if 'inspect($0)' was evaluated");
    ok(webconsoleUI.jsterm.history[0] === 'inspect($0)');
    yield toolbox.toggleSplitConsole();
  }
  function* testDuplicateNode() {
    info("Testing 'Duplicate Node' menu item for normal elements.");

    yield selectNode(".duplicate", inspector);
    is((yield testActor.getNumberOfElementMatches(".duplicate")), 1,
       "There should initially be 1 .duplicate node");

    let menuItem = inspector.panelDoc.getElementById("node-menu-duplicatenode");
    ok(menuItem, "'Duplicate node' menu item should exist");

    info("Triggering 'Duplicate Node' and waiting for inspector to update");
    let updated = inspector.once("markupmutation");
    dispatchCommandEvent(menuItem);
    yield updated;

    is((yield testActor.getNumberOfElementMatches(".duplicate")), 2,
       "The duplicated node should be in the markup.");

    let container = yield getContainerForSelector(".duplicate + .duplicate",
                                                   inspector);
    ok(container, "A MarkupContainer should be created for the new node");
  }

  function* testDeleteNode() {
    info("Testing 'Delete Node' menu item for normal elements.");
    yield selectNode("#delete", inspector);
    let deleteNode = inspector.panelDoc.getElementById("node-menu-delete");
    ok(deleteNode, "the popup menu has a delete menu item");
    let updated = inspector.once("inspector-updated");

    info("Triggering 'Delete Node' and waiting for inspector to update");
    dispatchCommandEvent(deleteNode);
    yield updated;

    ok(!(yield testActor.hasNode("#delete")), "Node deleted");
  }

  function* testDeleteRootNode() {
    info("Testing 'Delete Node' menu item does not delete root node.");
    yield selectNode("html", inspector);

    let deleteNode = inspector.panelDoc.getElementById("node-menu-delete");
    dispatchCommandEvent(deleteNode);

    let deferred = promise.defer();
    executeSoon(deferred.resolve);
    yield deferred.promise;

    ok((yield testActor.eval("!!content.document.documentElement")),
       "Document element still alive.");
  }

  function* testScrollIntoView() {
    // Follow up bug to add this test - https://bugzilla.mozilla.org/show_bug.cgi?id=1154107
    todo(false, "Verify that node is scrolled into the viewport.");
  }
});
