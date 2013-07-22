/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */


function test() {

  waitForExplicitFinish();

  let doc;
  let inspector;

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onload() {
    gBrowser.selectedBrowser.removeEventListener("load", onload, true);
    doc = content.document;
    waitForFocus(setupTest, content);
  }, true);

  content.location = "http://mochi.test:8888/browser/browser/devtools/" +
                     "inspector/test/browser_inspector_menu.html";

  function setupTest() {
    openInspector(runTests);
  }

  function runTests(aInspector) {
    inspector = aInspector;
    checkDocTypeMenuItems();
  }

  function checkDocTypeMenuItems() {
    info("Checking context menu entries for doctype node");
    inspector.selection.setNode(doc.doctype);
    inspector.once("inspector-updated", () => {
      let docTypeNode = getMarkupTagNodeContaining("<!DOCTYPE html>");

      // Right-click doctype tag
      contextMenuClick(docTypeNode);

      checkDisabled("node-menu-copyinner");
      checkDisabled("node-menu-copyouter");
      checkDisabled("node-menu-copyuniqueselector");
      checkDisabled("node-menu-delete");

      for (let name of ["hover", "active", "focus"]) {
        checkDisabled("node-menu-pseudo-" + name);
      }

      checkElementMenuItems();
    });
  }

  function checkElementMenuItems() {
    info("Checking context menu entries for p tag");
    inspector.selection.setNode(doc.querySelector("p"));
    inspector.once("inspector-updated", () => {
      let tag = getMarkupTagNodeContaining("p");

      // Right-click p tag
      contextMenuClick(tag);

      checkEnabled("node-menu-copyinner");
      checkEnabled("node-menu-copyouter");
      checkEnabled("node-menu-copyuniqueselector");
      checkEnabled("node-menu-delete");

      for (let name of ["hover", "active", "focus"]) {
        checkEnabled("node-menu-pseudo-" + name);
      }

      testCopyInnerMenu();
    });
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

    inspector.once("markupmutation", deleteTest);

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

  function getMarkupTagNodeContaining(text) {
    let tags = inspector._markupFrame.contentDocument.querySelectorAll("span");
    for (let tag of tags) {
      if (tag.textContent == text) {
        return tag;
      }
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

  function contextMenuClick(element) {
    let evt = element.ownerDocument.createEvent('MouseEvents');
    let button = 2;  // right click

    evt.initMouseEvent('contextmenu', true, true,
         element.ownerDocument.defaultView, 1, 0, 0, 0, 0, false,
         false, false, false, button, null);

    element.dispatchEvent(evt);
  }
}
