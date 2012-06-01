/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */


function test() {

  waitForExplicitFinish();

  let doc;

  let keySequence = "right down right ";
  keySequence += "down down down down right ";
  keySequence += "down down down right ";
  keySequence += "down down down down down right ";
  keySequence += "down down down down down ";
  keySequence += "up up up left down home ";
  keySequence += "pagedown left down down pageup pageup left down";

  keySequence = keySequence.split(" ");

  let keySequenceRes = "body node0 node0 ";
  keySequenceRes += "node1 node2 node3 node4 node4 ";
  keySequenceRes += "node5 node6 node7 node7 ";
  keySequenceRes += "node8 node9 node10 node11 node12 node12 ";
  keySequenceRes += "node13 node14 node15 node15 node15 ";
  keySequenceRes += "node14 node13 node12 node12 node14 html ";
  keySequenceRes += "node7 node7 node9 node10 body html html html";

  keySequenceRes = keySequenceRes.split(" ");


  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onload() {
    gBrowser.selectedBrowser.removeEventListener("load", onload, true);
    doc = content.document;
    waitForFocus(setupTest, content);
  }, true);

  content.location = "http://mochi.test:8888/browser/browser/devtools/highlighter/test/browser_inspector_treePanel_navigation.html";

  function setupTest() {
    Services.obs.addObserver(runTests, InspectorUI.INSPECTOR_NOTIFICATIONS.OPENED, false);
    InspectorUI.toggleInspectorUI();
  }

  function runTests() {
    Services.obs.removeObserver(runTests, InspectorUI.INSPECTOR_NOTIFICATIONS.OPENED);
    Services.obs.addObserver(startNavigation, InspectorUI.INSPECTOR_NOTIFICATIONS.TREEPANELREADY, false);
    InspectorUI.select(doc.body, true, true, true);
    InspectorUI.toggleHTMLPanel();
  }

  function startNavigation() {
    Services.obs.removeObserver(startNavigation, InspectorUI.INSPECTOR_NOTIFICATIONS.TREEPANELREADY);
    nextStep(0);
  }

  function nextStep(cursor) {
    let key = keySequence[cursor];
    let className = keySequenceRes[cursor];
    switch(key) {
      case "right":
        EventUtils.synthesizeKey("VK_RIGHT", {});
        break;
      case "down":
        EventUtils.synthesizeKey("VK_DOWN", {});
        break;
      case "left":
        EventUtils.synthesizeKey("VK_LEFT", {});
        break;
      case "up":
        EventUtils.synthesizeKey("VK_UP", {});
        break;
      case "pageup":
        EventUtils.synthesizeKey("VK_PAGE_UP", {});
        break;
      case "pagedown":
        EventUtils.synthesizeKey("VK_PAGE_DOWN", {});
        break;
      case "home":
        EventUtils.synthesizeKey("VK_HOME", {});
        break;
    }

    executeSoon(function() {
      if (cursor >= keySequence.length) {
        Services.obs.addObserver(finishUp, InspectorUI.INSPECTOR_NOTIFICATIONS.CLOSED, false);
        InspectorUI.closeInspectorUI();
      } else {
        let node = InspectorUI.treePanel.ioBox.selectedObjectBox.repObject;
        is(node.className, className, "[" + cursor + "] right node selected: " + className);
        nextStep(cursor + 1);
      }
    });
  }

  function finishUp() {
    Services.obs.removeObserver(finishUp, InspectorUI.INSPECTOR_NOTIFICATIONS.CLOSED);
    doc = null;
    gBrowser.removeCurrentTab();
    finish();
  }
}
