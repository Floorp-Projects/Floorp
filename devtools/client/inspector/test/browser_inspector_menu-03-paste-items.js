/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that different paste items work in the context menu

const TEST_URL = URL_ROOT + "doc_inspector_menu.html";
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

var clipboard = require("devtools/shared/platform/clipboard");
registerCleanupFunction(() => {
  clipboard = null;
});

add_task(async function() {
  let { inspector, testActor } = await openInspectorForURL(TEST_URL);

  await testPasteOuterHTMLMenu();
  await testPasteInnerHTMLMenu();
  await testPasteAdjacentHTMLMenu();

  async function testPasteOuterHTMLMenu() {
    info("Testing that 'Paste Outer HTML' menu item works.");

    await SimpleTest.promiseClipboardChange("this was pasted (outerHTML)",
      () => {
        clipboard.copyString("this was pasted (outerHTML)");
      });

    let outerHTMLSelector = "#paste-area h1";

    let nodeFront = await getNodeFront(outerHTMLSelector, inspector);
    await selectNode(nodeFront, inspector);

    let allMenuItems = openContextMenuAndGetAllItems(inspector, {
      target: getContainerForNodeFront(nodeFront, inspector).tagLine,
    });

    let onNodeReselected = inspector.markup.once("reselectedonremoved");
    allMenuItems.find(item => item.id === "node-menu-pasteouterhtml").click();

    info("Waiting for inspector selection to update");
    await onNodeReselected;

    let outerHTML = await testActor.getProperty("body", "outerHTML");
    ok(outerHTML.includes(clipboard.getText()),
       "Clipboard content was pasted into the node's outer HTML.");
    ok(!(await testActor.hasNode(outerHTMLSelector)),
      "The original node was removed.");
  }

  async function testPasteInnerHTMLMenu() {
    info("Testing that 'Paste Inner HTML' menu item works.");

    await SimpleTest.promiseClipboardChange("this was pasted (innerHTML)",
      () => {
        clipboard.copyString("this was pasted (innerHTML)");
      });
    let innerHTMLSelector = "#paste-area .inner";
    let getInnerHTML = () => testActor.getProperty(innerHTMLSelector,
                                                   "innerHTML");
    let origInnerHTML = await getInnerHTML();

    let nodeFront = await getNodeFront(innerHTMLSelector, inspector);
    await selectNode(nodeFront, inspector);

    let allMenuItems = openContextMenuAndGetAllItems(inspector, {
      target: getContainerForNodeFront(nodeFront, inspector).tagLine,
    });

    let onMutation = inspector.once("markupmutation");
    allMenuItems.find(item => item.id === "node-menu-pasteinnerhtml").click();
    info("Waiting for mutation to occur");
    await onMutation;

    ok((await getInnerHTML()) === clipboard.getText(),
       "Clipboard content was pasted into the node's inner HTML.");
    ok((await testActor.hasNode(innerHTMLSelector)),
       "The original node has been preserved.");
    await undoChange(inspector);
    ok((await getInnerHTML()) === origInnerHTML,
       "Previous innerHTML has been restored after undo");
  }

  async function testPasteAdjacentHTMLMenu() {
    let refSelector = "#paste-area .adjacent .ref";
    let adjacentNodeSelector = "#paste-area .adjacent";
    let nodeFront = await getNodeFront(refSelector, inspector);
    await selectNode(nodeFront, inspector);
    let markupTagLine = getContainerForNodeFront(nodeFront, inspector).tagLine;

    for (let { clipboardData, menuId } of PASTE_ADJACENT_HTML_DATA) {
      let allMenuItems = openContextMenuAndGetAllItems(inspector, {
        target: markupTagLine,
      });
      info(`Testing ${menuId} for ${clipboardData}`);

      await SimpleTest.promiseClipboardChange(clipboardData,
        () => {
          clipboard.copyString(clipboardData);
        });

      let onMutation = inspector.once("markupmutation");
      allMenuItems.find(item => item.id === menuId).click();
      info("Waiting for mutation to occur");
      await onMutation;
    }

    let html = await testActor.getProperty(adjacentNodeSelector, "innerHTML");
    ok(html.trim() === "1<span class=\"ref\">234</span><span>5</span>",
       "The Paste as Last Child / as First Child / Before / After worked as " +
       "expected");
    await undoChange(inspector);

    html = await testActor.getProperty(adjacentNodeSelector, "innerHTML");
    ok(html.trim() === "1<span class=\"ref\">234</span>",
       "Undo works for paste adjacent HTML");
  }
});
