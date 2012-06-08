/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */


function test() {

  waitForExplicitFinish();

  let doc;
  let node1;
  let div;

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
    setupTest();
  }

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onload() {
    gBrowser.selectedBrowser.removeEventListener("load", onload, true);
    doc = content.document;
    waitForFocus(createDocument, content);
  }, true);

  content.location = content.location = "data:text/html,basic tests for inspector";;

  function setupTest() {
    Services.obs.addObserver(runTests, InspectorUI.INSPECTOR_NOTIFICATIONS.OPENED, false);
    InspectorUI.toggleInspectorUI();
  }

  function runTests() {
    Services.obs.removeObserver(runTests, InspectorUI.INSPECTOR_NOTIFICATIONS.OPENED);
    Services.obs.addObserver(testCopyInnerMenu, InspectorUI.INSPECTOR_NOTIFICATIONS.TREEPANELREADY, false);
    InspectorUI.stopInspecting();
    InspectorUI.inspectNode(node1, true);
    InspectorUI.treePanel.open();
  }

  function testCopyInnerMenu() {
    let copyInner = document.getElementById("inspectorHTMLCopyInner");
    ok(copyInner, "the popup menu has a copy inner html menu item");

    waitForClipboard("This is some example text",
                     function() { copyInner.doCommand(); },
                     testCopyOuterMenu, testCopyOuterMenu);
  }

  function testCopyOuterMenu() {
    let copyOuter = document.getElementById("inspectorHTMLCopyOuter");
    ok(copyOuter, "the popup menu has a copy outer html menu item");

    waitForClipboard("<p>This is some example text</p>",
                     function() { copyOuter.doCommand(); },
                     testDeleteNode, testDeleteNode);
  }

  function testDeleteNode() {
    let deleteNode = document.getElementById("inspectorHTMLDelete");
    ok(deleteNode, "the popup menu has a delete menu item");

    InspectorUI.highlighter.addListener("nodeselected", deleteTest);

    let commandEvent = document.createEvent("XULCommandEvent");
    commandEvent.initCommandEvent("command", true, true, window, 0, false, false,
                                  false, false, null);
    deleteNode.dispatchEvent(commandEvent);
  }

  function deleteTest() {
    InspectorUI.highlighter.removeListener("nodeselected", deleteTest);
    is(InspectorUI.selection, div, "parent node selected");
    let p = doc.querySelector("P");
    is(p, null, "node deleted");

    InspectorUI.highlighter.addListener("nodeselected", deleteRootNode);
    InspectorUI.inspectNode(doc.documentElement, true);
  }

  function deleteRootNode() {
    InspectorUI.highlighter.removeListener("nodeselected", deleteRootNode);
    let deleteNode = document.getElementById("inspectorHTMLDelete");
    let commandEvent = document.createEvent("XULCommandEvent");
    commandEvent.initCommandEvent("command", true, true, window, 0, false, false,
                                  false, false, null);
    deleteNode.dispatchEvent(commandEvent);
    executeSoon(isRootStillAlive);
  }

  function isRootStillAlive() {
    ok(doc.documentElement, "Document element still alive.");
    Services.obs.addObserver(finishUp, InspectorUI.INSPECTOR_NOTIFICATIONS.CLOSED, false);
    executeSoon(function() {
      InspectorUI.closeInspectorUI();
    });
  }

  function finishUp() {
    Services.obs.removeObserver(finishUp, InspectorUI.INSPECTOR_NOTIFICATIONS.CLOSED);
    doc = node1 = div = null;
    gBrowser.removeCurrentTab();
    finish();
  }
}
