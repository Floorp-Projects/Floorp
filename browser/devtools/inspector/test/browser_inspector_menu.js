/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */


function test() {

  waitForExplicitFinish();

  let doc;
  let node1;
  let div;
  let inspector;

  function createDocument() {
    div = doc.createElement("div");
    let h1 = doc.createElement("h1");
    let p1 = doc.createElement("p");
    let p2 = doc.createElement("p");
    doc.title = "Inspector Tree Menu Test";
    h1.textContent = "Inspector Tree Menu Test";
    p1.textContent = "This is some example text";
    div.appendChild(h1);
    div.appendChild(p1);
    doc.body.appendChild(div);
    node1 = p1;
    openInspector(runTests);
  }

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onload() {
    gBrowser.selectedBrowser.removeEventListener("load", onload, true);
    doc = content.document;
    waitForFocus(createDocument, content);
  }, true);

  content.location = content.location = "data:text/html,basic tests for inspector";;

  function runTests(aInspector) {
    inspector = aInspector;
    inspector.selection.setNode(node1);
    testCopyInnerMenu();
  }

  function testCopyInnerMenu() {
    let copyInner = inspector.panelDoc.getElementById("node-menu-copyinner");
    ok(copyInner, "the popup menu has a copy inner html menu item");

    waitForClipboard("This is some example text",
                     function() { copyInner.doCommand(); },
                     testCopyOuterMenu, testCopyOuterMenu);
  }

  function testCopyOuterMenu() {
    let copyOuter = inspector.panelDoc.getElementById("node-menu-copyouter");
    ok(copyOuter, "the popup menu has a copy outer html menu item");

    waitForClipboard("<p>This is some example text</p>",
                     function() { copyOuter.doCommand(); },
                     testCopyUniqueSelectorMenu, testCopyUniqueSelectorMenu);
  }

  function testCopyUniqueSelectorMenu() {
    let copyUniqueSelector = inspector.panelDoc.getElementById("node-menu-copyuniqueselector");
    ok(copyUniqueSelector, "the popup menu has a copy unique selector menu item");

    waitForClipboard("body > div:nth-child(1) > p:nth-child(2)",
                     function() { copyUniqueSelector.doCommand(); },
                     testDeleteNode, testDeleteNode);
  }

  function testDeleteNode() {
    let deleteNode = inspector.panelDoc.getElementById("node-menu-delete");
    ok(deleteNode, "the popup menu has a delete menu item");

    inspector.selection.once("detached", deleteTest);

    let commandEvent = document.createEvent("XULCommandEvent");
    commandEvent.initCommandEvent("command", true, true, window, 0, false, false,
                                  false, false, null);
    deleteNode.dispatchEvent(commandEvent);
  }

  function deleteTest() {
    let p = doc.querySelector("P");
    is(p, null, "node deleted");

    deleteRootNode();
  }

  function deleteRootNode() {
    inspector.selection.setNode(doc.documentElement);
    let deleteNode = inspector.panelDoc.getElementById("node-menu-delete");
    let commandEvent = inspector.panelDoc.createEvent("XULCommandEvent");
    commandEvent.initCommandEvent("command", true, true, window, 0, false, false,
                                  false, false, null);
    deleteNode.dispatchEvent(commandEvent);
    executeSoon(isRootStillAlive);
  }

  function isRootStillAlive() {
    ok(doc.documentElement, "Document element still alive.");
    gBrowser.removeCurrentTab();
    finish();
  }
}
