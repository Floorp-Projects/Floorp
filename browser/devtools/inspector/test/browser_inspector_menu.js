/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test context menu functionality:
// 1) menu items are disabled/enabled depending on the clicked node
// 2) actions triggered by the items work correctly

const TEST_URL = TEST_URL_ROOT + "doc_inspector_menu.html";
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

const PASTE_OUTER_HTML_TEST_DATA = [
  {
    desc: "some text",
    clipboardData: "some text",
    clipboardDataType: undefined,
    disabled: false
  },
  {
    desc: "base64 encoded image data uri",
    clipboardData:
      "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABC" +
      "AAAAAA6fptVAAAACklEQVQYV2P4DwABAQEAWk1v8QAAAABJRU5ErkJggg==",
    clipboardDataType: undefined,
    disabled: true
  },
  {
    desc: "html",
    clipboardData: "<p>some text</p>",
    clipboardDataType: "html",
    disabled: false
  },
  {
    desc: "empty string",
    clipboardData: "",
    clipboardDataType: undefined,
    disabled: true
  },
  {
    desc: "whitespace only",
    clipboardData: " \n\n\t\n\n  \n",
    clipboardDataType: undefined,
    disabled: true
  },
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

let clipboard = devtools.require("sdk/clipboard");
registerCleanupFunction(() => {
  clipboard = null;
});

let test = asyncTest(function* () {
  let { inspector, toolbox } = yield openInspectorForURL(TEST_URL);

  yield testMenuItemSensitivity();
  yield testPasteOuterHTMLMenuItemSensitivity();
  yield testCopyMenuItems();
  yield testShowDOMProperties();
  yield testPasteOuterHTMLMenu();
  yield testDeleteNode();
  yield testDeleteRootNode();

  function* testMenuItemSensitivity() {
    info("Testing sensitivity of menu items for different elements.");

    const MENU_ITEMS = [
      "node-menu-copyinner",
      "node-menu-copyouter",
      "node-menu-copyuniqueselector",
      "node-menu-delete",
      "node-menu-pasteouterhtml",
      "node-menu-pseudo-hover",
      "node-menu-pseudo-active",
      "node-menu-pseudo-focus"
    ];

    // To ensure clipboard contains something to paste.
    clipboard.set("<p>test</p>", "html");

    for (let {desc, selector, disabled} of MENU_SENSITIVITY_TEST_DATA) {
      info("Testing context menu entries for " + desc);

      let node = getNode(selector) || content.document.doctype;
      yield selectNode(node, inspector);

      contextMenuClick(getContainerForRawNode(inspector.markup, node).tagLine);

      for (let name of MENU_ITEMS) {
        checkMenuItem(name, disabled);
      }
    }
  }

  function* testPasteOuterHTMLMenuItemSensitivity() {
    info("Checking 'Paste Outer HTML' menu item sensitivity for different types" +
         "of data");

    let node = getNode("p");
    let markupTagLine = getContainerForRawNode(inspector.markup, node).tagLine;

    for (let data of PASTE_OUTER_HTML_TEST_DATA) {
      let { desc, clipboardData, clipboardDataType, disabled } = data;
      info("Checking 'Paste Outer HTML' for " + desc);
      clipboard.set(clipboardData, clipboardDataType);

      yield selectNode(node, inspector);

      contextMenuClick(markupTagLine);
      checkMenuItem("node-menu-pasteouterhtml", disabled);
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

  function* testPasteOuterHTMLMenu() {
    info("Testing that 'Paste Outer HTML' menu item works.");
    clipboard.set("this was pasted");

    let node = getNode("h1");
    yield selectNode(node, inspector);

    contextMenuClick(getContainerForRawNode(inspector.markup, node).tagLine);

    let onNodeReselected = inspector.markup.once("reselectedonremoved");
    let menu = inspector.panelDoc.getElementById("node-menu-pasteouterhtml");
    dispatchCommandEvent(menu);

    info("Waiting for inspector selection to update");
    yield onNodeReselected;

    ok(content.document.body.outerHTML.contains(clipboard.get()),
       "Clipboard content was pasted into the node's outer HTML.");
    ok(!getNode("h1", { expectNoMatch: true }), "The original node was removed.");
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
    yield selectNode(content.document.documentElement, inspector);

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
