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

  content.location = "data:text/html,<div><p></p></div>";

  function setupTest() {
    node1 = doc.querySelector("div");
    node2 = doc.querySelector("p");
    Services.obs.addObserver(runTests, INSPECTOR_NOTIFICATIONS.OPENED, false);
    InspectorUI.toggleInspectorUI();
  }

  function runTests() {
    Services.obs.removeObserver(runTests, INSPECTOR_NOTIFICATIONS.OPENED);
    testNode1();
  }

  function testNode1() {
    let box = InspectorUI.ioBox.createObjectBox(node1);
    box.click();
    executeSoon(function() {
      is(InspectorUI.selection, node1, "selection matches node");
      is(InspectorUI.highlighter.node, node1, "selection matches node");
      testNode2();
    });
  }

  function testNode2() {
    let box = InspectorUI.ioBox.createObjectBox(node2);
    box.click();
    executeSoon(function() {
      is(InspectorUI.selection, node2, "selection matches node");
      is(InspectorUI.highlighter.node, node2, "selection matches node");
      Services.obs.addObserver(finishUp, INSPECTOR_NOTIFICATIONS.CLOSED, false);
      InspectorUI.closeInspectorUI();
    });
  }

  function finishUp() {
    Services.obs.removeObserver(finishUp, INSPECTOR_NOTIFICATIONS.CLOSED);
    doc = node1 = node2 = null;
    gBrowser.removeCurrentTab();
    finish();
  }
}
