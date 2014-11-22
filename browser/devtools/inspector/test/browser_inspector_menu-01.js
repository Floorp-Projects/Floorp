/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

///////////////////
//
// Whitelisting this test.
// As part of bug 1077403, the leaking uncaught rejection should be fixed.
//
thisTestLeaksUncaughtRejectionsAndShouldBeFixed("TypeError: jsterm.focusInput is not a function");

// Test context menu functionality:
// 1) menu items are disabled/enabled depending on the clicked node
// 2) actions triggered by the items work correctly

const TEST_URL = TEST_URL_ROOT + "doc_inspector_menu-01.html";
const MENU_SENSITIVITY_TEST_DATA = [
  {
    desc: "doctype node",
    selector: null,
    disabled: true,
  },
  {
    desc: "element node",
    selector: "p",
    disabled: false,
  }
];

const COPY_ITEMS_TEST_DATA = [
  {
    desc: "copy inner html",
    id: "node-menu-copyinner",
    text: "This is some example text",
  },
  {
    desc: "copy outer html",
    id: "node-menu-copyouter",
    text: "<p>This is some example text</p>",
  },
  {
    desc: "copy unique selector",
    id: "node-menu-copyuniqueselector",
    text: "body > div:nth-child(1) > p:nth-child(2)",
  },
];

let clipboard = require("sdk/clipboard");
registerCleanupFunction(() => {
  clipboard = null;
});

add_task(function* () {
  let { inspector, toolbox } = yield openInspectorForURL(TEST_URL);

  yield testMenuItemSensitivity();
  yield testCopyMenuItems();
  yield testShowDOMProperties();
  yield testDeleteNode();
  yield testDeleteRootNode();

  function* testMenuItemSensitivity() {
    info("Testing sensitivity of menu items for different elements.");

    // The sensibility for paste options are described in browser_inspector_menu-02.js
    const MENU_ITEMS = [
      "node-menu-copyinner",
      "node-menu-copyouter",
      "node-menu-copyuniqueselector",
      "node-menu-delete",
      "node-menu-pseudo-hover",
      "node-menu-pseudo-active",
      "node-menu-pseudo-focus"
    ];

    for (let {desc, selector, disabled} of MENU_SENSITIVITY_TEST_DATA) {
      info("Testing context menu entries for " + desc);

      let front;
      if (selector) {
        front = yield getNodeFront(selector, inspector);
      } else {
        // Select the docType if no selector is provided
        let {nodes} = yield inspector.walker.children(inspector.walker.rootNode);
        front = nodes[0];
      }
      yield selectNode(front, inspector);

      contextMenuClick(getContainerForNodeFront(front, inspector).tagLine);

      for (let name of MENU_ITEMS) {
        checkMenuItem(name, disabled);
      }
    }
  }

  function* testCopyMenuItems() {
    info("Testing various copy actions of context menu.");
    for (let {desc, id, text} of COPY_ITEMS_TEST_DATA) {
      info("Testing " + desc);

      let item = inspector.panelDoc.getElementById(id);
      ok(item, "The popup has a " + desc + " menu item.");

      let deferred = promise.defer();
      waitForClipboard(text, () => item.doCommand(),
                       deferred.resolve, deferred.reject);
      yield deferred.promise;
    }
  }

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

    yield selectNode("p", inspector);
    let deleteNode = inspector.panelDoc.getElementById("node-menu-delete");
    ok(deleteNode, "the popup menu has a delete menu item");

    let updated = inspector.once("inspector-updated");

    info("Triggering 'Delete Node' and waiting for inspector to update");
    dispatchCommandEvent(deleteNode);
    yield updated;

    ok(!getNode("p", { expectNoMatch: true }), "Node deleted");
  }

  function* testDeleteRootNode() {
    info("Testing 'Delete Node' menu item does not delete root node.");
    yield selectNode(inspector.walker.rootNode, inspector);

    let deleteNode = inspector.panelDoc.getElementById("node-menu-delete");
    dispatchCommandEvent(deleteNode);

    executeSoon(() => {
      ok(content.document.documentElement, "Document element still alive.");
    });
  }

  function checkMenuItem(elementId, disabled) {
    if (disabled) {
      checkDisabled(elementId);
    } else {
      checkEnabled(elementId);
    }
  }

  function checkEnabled(elementId) {
    let elt = inspector.panelDoc.getElementById(elementId);
    ok(!elt.hasAttribute("disabled"),
      '"' + elt.label + '" context menu option is not disabled');
  }

  function checkDisabled(elementId) {
    let elt = inspector.panelDoc.getElementById(elementId);
    ok(elt.hasAttribute("disabled"),
      '"' + elt.label + '" context menu option is disabled');
  }

  function dispatchCommandEvent(node) {
    info("Dispatching command event on " + node);
    let commandEvent = document.createEvent("XULCommandEvent");
    commandEvent.initCommandEvent("command", true, true, window, 0, false, false,
                                  false, false, null);
    node.dispatchEvent(commandEvent);
  }

  function contextMenuClick(element) {
    info("Simulating contextmenu event on " + element);
    let evt = element.ownerDocument.createEvent('MouseEvents');
    let button = 2;  // right click

    evt.initMouseEvent('contextmenu', true, true,
         element.ownerDocument.defaultView, 1, 0, 0, 0, 0, false,
         false, false, false, button, null);

    element.dispatchEvent(evt);
  }
});
