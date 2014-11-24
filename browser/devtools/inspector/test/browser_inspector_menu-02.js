/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test context menu functionality:
// 1) menu items are disabled/enabled depending on the clicked node
// 2) actions triggered by the items work correctly

///////////////////
//
// Whitelisting this test.
// As part of bug 1077403, the leaking uncaught rejection should be fixed.
//
thisTestLeaksUncaughtRejectionsAndShouldBeFixed("TypeError: jsterm.focusInput is not a function");

const MENU_SENSITIVITY_TEST_DATA = [
  {
    desc: "doctype node",
    selector: null,
    disabled: true,
  },
  {
    desc: "element node",
    selector: "#sensitivity",
    disabled: false,
  },
  {
    desc: "document element",
    selector: "html",
    disabled: {
      "node-menu-pastebefore": true,
      "node-menu-pasteafter": true,
      "node-menu-pastefirstchild": true,
      "node-menu-pastelastchild": true,
    }
  },
  {
    desc: "body",
    selector: "body",
    disabled: {
      "node-menu-pastebefore": true,
      "node-menu-pasteafter": true,
    }
  },
  {
    desc: "head",
    selector: "head",
    disabled: {
      "node-menu-pastebefore": true,
      "node-menu-pasteafter": true,
    }
  }
];

const TEST_URL = TEST_URL_ROOT + "doc_inspector_menu-02.html";

const PASTE_HTML_TEST_SENSITIVITY_DATA = [
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

const PASTE_ADJACENT_HTML_DATA = [
  {
    desc: "As First Child",
    clipboardData: "2",
    menuId: "node-menu-pastefirstchild",
  },
  {
    desc: "As Last Child",
    clipboardData: "4",
    menuId: "node-menu-pastelastchild",
  },
  {
    desc: "Before",
    clipboardData: "1",
    menuId: "node-menu-pastebefore",
  },
  {
    desc: "After",
    clipboardData: "<span>5</span>",
    menuId: "node-menu-pasteafter",
  },
];


let clipboard = require("sdk/clipboard");
registerCleanupFunction(() => {
  clipboard = null;
});

add_task(function* () {
  let { inspector, toolbox } = yield openInspectorForURL(TEST_URL);

  yield testMenuItemSensitivity();
  yield testPasteHTMLMenuItemsSensitivity();
  yield testPasteOuterHTMLMenu();
  yield testPasteInnerHTMLMenu();
  yield testPasteAdjacentHTMLMenu();

  function* testMenuItemSensitivity() {
    info("Testing sensitivity of menu items for different elements.");

    const MENU_ITEMS = [
      "node-menu-pasteinnerhtml",
      "node-menu-pasteouterhtml",
      "node-menu-pastebefore",
      "node-menu-pasteafter",
      "node-menu-pastefirstchild",
      "node-menu-pastelastchild",
    ];

    // To ensure clipboard contains something to paste.
    clipboard.set("<p>test</p>", "html");

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
        let disabledForMenu = typeof disabled === "object" ?
          disabled[name] : disabled;
        info(`${name} should be ${disabledForMenu ? "disabled" : "enabled"}  ` +
          `for ${desc}`);
        checkMenuItem(name, disabledForMenu);
      }
    }
  }

  function* testPasteHTMLMenuItemsSensitivity() {
    let menus = [
      "node-menu-pasteinnerhtml",
      "node-menu-pasteouterhtml",
      "node-menu-pastebefore",
      "node-menu-pasteafter",
      "node-menu-pastefirstchild",
      "node-menu-pastelastchild",
    ];

    info("Checking Paste menu items sensitivity for different types" +
         "of data");

    let nodeFront = yield getNodeFront("#paste-area", inspector);
    let markupTagLine = getContainerForNodeFront(nodeFront, inspector).tagLine;

    for (let menuId of menus) {
      for (let data of PASTE_HTML_TEST_SENSITIVITY_DATA) {
        let { desc, clipboardData, clipboardDataType, disabled } = data;
        let menuLabel = getLabelFor("#" + menuId);
        info(`Checking ${menuLabel} for ${desc}`);
        clipboard.set(clipboardData, clipboardDataType);

        yield selectNode(nodeFront, inspector);

        contextMenuClick(markupTagLine);
        checkMenuItem(menuId, disabled);
      }
    }
  }

  function* testPasteOuterHTMLMenu() {
    info("Testing that 'Paste Outer HTML' menu item works.");
    clipboard.set("this was pasted (outerHTML)");
    let outerHTMLSelector = "#paste-area h1";

    let nodeFront = yield getNodeFront(outerHTMLSelector, inspector);
    yield selectNode(nodeFront, inspector);

    contextMenuClick(getContainerForNodeFront(nodeFront, inspector).tagLine);

    let onNodeReselected = inspector.markup.once("reselectedonremoved");
    let menu = inspector.panelDoc.getElementById("node-menu-pasteouterhtml");
    dispatchCommandEvent(menu);

    info("Waiting for inspector selection to update");
    yield onNodeReselected;

    ok(content.document.body.outerHTML.contains(clipboard.get()),
       "Clipboard content was pasted into the node's outer HTML.");
    ok(!getNode(outerHTMLSelector, { expectNoMatch: true }),
      "The original node was removed.");
  }

  function* testPasteInnerHTMLMenu() {
    info("Testing that 'Paste Inner HTML' menu item works.");
    clipboard.set("this was pasted (innerHTML)");
    let innerHTMLSelector = "#paste-area .inner";
    let getInnerHTML = () => content.document.querySelector(innerHTMLSelector).innerHTML;
    let origInnerHTML = getInnerHTML();

    let nodeFront = yield getNodeFront(innerHTMLSelector, inspector);
    yield selectNode(nodeFront, inspector);

    contextMenuClick(getContainerForNodeFront(nodeFront, inspector).tagLine);

    let onMutation = inspector.once("markupmutation");
    let menu = inspector.panelDoc.getElementById("node-menu-pasteinnerhtml");
    dispatchCommandEvent(menu);

    info("Waiting for mutation to occur");
    yield onMutation;

    ok(getInnerHTML() === clipboard.get(),
       "Clipboard content was pasted into the node's inner HTML.");
    ok(getNode(innerHTMLSelector), "The original node has been preserved.");
    yield undoChange(inspector);
    ok(getInnerHTML() === origInnerHTML, "Previous innerHTML has been " +
      "restored after undo");
  }

  function* testPasteAdjacentHTMLMenu() {
    let refSelector = "#paste-area .adjacent .ref";
    let adjacentNode = content.document.querySelector(refSelector).parentNode;
    let nodeFront = yield getNodeFront(refSelector, inspector);
    yield selectNode(nodeFront, inspector);
    let markupTagLine = getContainerForNodeFront(nodeFront, inspector).tagLine;

    for (let { desc, clipboardData, menuId } of PASTE_ADJACENT_HTML_DATA) {
      let menu = inspector.panelDoc.getElementById(menuId);
      info(`Testing ${getLabelFor(menu)} for ${clipboardData}`);
      clipboard.set(clipboardData);

      contextMenuClick(markupTagLine);
      let onMutation = inspector.once("markupmutation");
      dispatchCommandEvent(menu);

      info("Waiting for mutation to occur");
      yield onMutation;
    }

    ok(adjacentNode.innerHTML.trim() === "1<span class=\"ref\">234</span>" +
      "<span>5</span>", "The Paste as Last Child / as First Child / Before " +
      "/ After worked as expected");
    yield undoChange(inspector);
    ok(adjacentNode.innerHTML.trim() === "1<span class=\"ref\">234</span>",
      "Undo works for paste adjacent HTML");
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

  function getLabelFor(elt) {
    if (typeof elt === "string")
      elt = inspector.panelDoc.querySelector(elt);
    let isInPasteSubMenu = elt.matches("#node-menu-paste-extra-submenu *");
    return `"${isInPasteSubMenu ? "Paste > " : ""}${elt.label}"`;
  }
});
