/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Tests for menuitem functionality that doesn't fit into any specific category

const TEST_URL = TEST_URL_ROOT + "doc_inspector_menu.html";

add_task(function* () {
  let { inspector, toolbox } = yield openInspectorForURL(TEST_URL);

  yield testShowDOMProperties();
  yield testDeleteNode();
  yield testDeleteRootNode();

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

  function* testDeleteNode() {
    info("Testing 'Delete Node' menu item for normal elements.");

    yield selectNode("#delete", inspector);
    let deleteNode = inspector.panelDoc.getElementById("node-menu-delete");
    ok(deleteNode, "the popup menu has a delete menu item");

    let updated = inspector.once("inspector-updated");

    info("Triggering 'Delete Node' and waiting for inspector to update");
    dispatchCommandEvent(deleteNode);
    yield updated;

    ok(!getNode("#delete", { expectNoMatch: true }), "Node deleted");
  }

  function* testDeleteRootNode() {
    info("Testing 'Delete Node' menu item does not delete root node.");
    yield selectNode("html", inspector);

    let deleteNode = inspector.panelDoc.getElementById("node-menu-delete");
    dispatchCommandEvent(deleteNode);

    executeSoon(() => {
      ok(content.document.documentElement, "Document element still alive.");
    });
  }

  function dispatchCommandEvent(node) {
    info("Dispatching command event on " + node);
    let commandEvent = document.createEvent("XULCommandEvent");
    commandEvent.initCommandEvent("command", true, true, window, 0, false, false,
                                  false, false, null);
    node.dispatchEvent(commandEvent);
  }
});
