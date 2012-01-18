/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */


function test() {

  waitForExplicitFinish();

  let doc;
  let node1;
  let node2;

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onload() {
    gBrowser.selectedBrowser.removeEventListener("load", onload, true);
    doc = content.document;
    waitForFocus(setupTest, content);
  }, true);

  content.location = 'data:text/html,<div style="width: 200px; height: 200px"><p></p></div>';

  function setupTest() {
    node1 = doc.querySelector("div");
    node2 = doc.querySelector("p");
    Services.obs.addObserver(runTests, InspectorUI.INSPECTOR_NOTIFICATIONS.OPENED, false);
    InspectorUI.toggleInspectorUI();
  }

  function runTests() {
    Services.obs.removeObserver(runTests, InspectorUI.INSPECTOR_NOTIFICATIONS.OPENED);
    Services.obs.addObserver(testNode1, InspectorUI.INSPECTOR_NOTIFICATIONS.TREEPANELREADY, false);
    InspectorUI.select(node1, true, true, true);
    InspectorUI.openTreePanel();
  }

  function testNode1() {
    Services.obs.removeObserver(testNode1, InspectorUI.INSPECTOR_NOTIFICATIONS.TREEPANELREADY);
    is(InspectorUI.selection, node1, "selection matches node");
    is(getHighlitNode(), node1, "selection matches node");
    testNode2();
  }

  function testNode2() {
    InspectorUI.highlighter.addListener("nodeselected", testHighlightingNode2);
    InspectorUI.treePanelSelect("node2");
  }

  function testHighlightingNode2() {
    InspectorUI.highlighter.removeListener("nodeselected", testHighlightingNode2);
    is(InspectorUI.selection, node2, "selection matches node");
    is(getHighlitNode(), node2, "selection matches node");
    Services.obs.addObserver(finishUp, InspectorUI.INSPECTOR_NOTIFICATIONS.CLOSED, false);
    InspectorUI.closeInspectorUI();
  }

  function finishUp() {
    Services.obs.removeObserver(finishUp, InspectorUI.INSPECTOR_NOTIFICATIONS.CLOSED);
    doc = node1 = node2 = null;
    gBrowser.removeCurrentTab();
    finish();
  }
}
