/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that different paste items work in the context menu


const TEST_URL = TEST_URL_ROOT + "doc_inspector_menu.html";
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
  let { inspector, testActor } = yield openInspectorForURL(TEST_URL);

  yield testPasteOuterHTMLMenu();
  yield testPasteInnerHTMLMenu();
  yield testPasteAdjacentHTMLMenu();

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

    let outerHTML = yield testActor.getProperty("body", "outerHTML");
    ok(outerHTML.includes(clipboard.get()),
       "Clipboard content was pasted into the node's outer HTML.");
    ok(!(yield testActor.hasNode(outerHTMLSelector)),
      "The original node was removed.");
  }

  function* testPasteInnerHTMLMenu() {
    info("Testing that 'Paste Inner HTML' menu item works.");
    clipboard.set("this was pasted (innerHTML)");
    let innerHTMLSelector = "#paste-area .inner";
    let getInnerHTML = () => testActor.getProperty(innerHTMLSelector, "innerHTML");
    let origInnerHTML = yield getInnerHTML();

    let nodeFront = yield getNodeFront(innerHTMLSelector, inspector);
    yield selectNode(nodeFront, inspector);

    contextMenuClick(getContainerForNodeFront(nodeFront, inspector).tagLine);

    let onMutation = inspector.once("markupmutation");
    let menu = inspector.panelDoc.getElementById("node-menu-pasteinnerhtml");
    dispatchCommandEvent(menu);

    info("Waiting for mutation to occur");
    yield onMutation;

    ok((yield getInnerHTML()) === clipboard.get(),
       "Clipboard content was pasted into the node's inner HTML.");
    ok((yield testActor.hasNode(innerHTMLSelector)), "The original node has been preserved.");
    yield undoChange(inspector);
    ok((yield getInnerHTML()) === origInnerHTML, "Previous innerHTML has been " +
      "restored after undo");
  }

  function* testPasteAdjacentHTMLMenu() {
    let refSelector = "#paste-area .adjacent .ref";
    let adjacentNodeSelector = "#paste-area .adjacent";
    let nodeFront = yield getNodeFront(refSelector, inspector);
    yield selectNode(nodeFront, inspector);
    let markupTagLine = getContainerForNodeFront(nodeFront, inspector).tagLine;

    for (let { clipboardData, menuId } of PASTE_ADJACENT_HTML_DATA) {
      let menu = inspector.panelDoc.getElementById(menuId);
      info(`Testing ${getLabelFor(menu)} for ${clipboardData}`);
      clipboard.set(clipboardData);

      contextMenuClick(markupTagLine);
      let onMutation = inspector.once("markupmutation");
      dispatchCommandEvent(menu);

      info("Waiting for mutation to occur");
      yield onMutation;
    }

    ok((yield testActor.getProperty(adjacentNodeSelector, "innerHTML")).trim() ===
      "1<span class=\"ref\">234</span>" +
      "<span>5</span>", "The Paste as Last Child / as First Child / Before " +
      "/ After worked as expected");
    yield undoChange(inspector);
    ok((yield testActor.getProperty(adjacentNodeSelector, "innerHTML")).trim() ===
      "1<span class=\"ref\">234</span>",
      "Undo works for paste adjacent HTML");
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
