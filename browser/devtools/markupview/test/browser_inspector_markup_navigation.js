/* Any copyright", " is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */


function test() {

  waitForExplicitFinish();

  let doc;

  let keySequences = [
    ["right", "body"],
    ["down", "node0"],
    ["right", "node0"],
    ["down", "node1"],
    ["down", "node2"],
    ["down", "node3"],
    ["down", "*comment*"],
    ["down", "node4"],
    ["right", "node4"],
    ["down", "*text*"],
    ["down", "node5"],
    ["down", "node6"],
    ["down", "*comment*"],
    ["down" , "node7"],
    ["right", "node7"],
    ["down", "*text*"],
    ["down", "node8"],
    ["down", "node9"],
    ["down", "node10"],
    ["down", "node11"],
    ["down", "node12"],
    ["right", "node12"],
    ["down", "*text*"],
    ["down", "node13"],
    ["down", "node14"],
    ["down", "node15"],
    ["down", "node15"],
    ["down", "node15"],
    ["up", "node14"],
    ["up", "node13"],
    ["up", "*text*"],
    ["up", "node12"],
    ["left", "node12"],
    ["down", "node14"],
    ["home", "*doctype*"],
    ["pagedown", "*text*"],
    ["down", "node5"],
    ["down", "node6"],
    ["down", "*comment*"],
    ["down", "node7"],
    ["left", "node7"],
    ["down", "node9"],
    ["down", "node10"],
    ["pageup", "node2"],
    ["pageup", "*doctype*"],
    ["down", "html"],
    ["left", "html"],
    ["down", "html"]
  ];

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onload() {
    gBrowser.selectedBrowser.removeEventListener("load", onload, true);
    doc = content.document;
    waitForFocus(setupTest, content);
  }, true);

  content.location = "http://mochi.test:8888/browser/browser/devtools/markupview/test/browser_inspector_markup_navigation.html";

  let markup = null;

  function setupTest() {
    Services.obs.addObserver(runTests, InspectorUI.INSPECTOR_NOTIFICATIONS.OPENED, false);
    InspectorUI.toggleInspectorUI();
  }

  function runTests() {
    Services.obs.removeObserver(runTests, InspectorUI.INSPECTOR_NOTIFICATIONS.OPENED);
    InspectorUI.currentInspector.once("markuploaded", startNavigation);
    InspectorUI.select(doc.body, true, true, true);
    InspectorUI.toggleHTMLPanel();
  }

  function startNavigation() {
    markup = InspectorUI.currentInspector.markup;
    nextStep(0);
  }

  function nextStep(cursor) {
    if (cursor >= keySequences.length) {
      Services.obs.addObserver(finishUp, InspectorUI.INSPECTOR_NOTIFICATIONS.CLOSED, false);
      InspectorUI.closeInspectorUI();
      return;
    }

    let key = keySequences[cursor][0];
    let className = keySequences[cursor][1];

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
      let node = markup.selected;
      if (className == "*comment*") {
        is(node.nodeType, Node.COMMENT_NODE, "[" + cursor + "] should be a comment after moving " + key);
      } else if (className == "*text*") {
        is(node.nodeType, Node.TEXT_NODE, "[" + cursor + "] should be text after moving " + key);
      } else if (className == "*doctype*") {
        is(node.nodeType, Node.DOCUMENT_TYPE_NODE, "[" + cursor + "] should be doctype after moving " + key);
      } else {
        is(node.className, className, "[" + cursor + "] right node selected: " + className + " after moving " + key);
      }
      nextStep(cursor + 1);
    });
  }

  function finishUp() {
    Services.obs.removeObserver(finishUp, InspectorUI.INSPECTOR_NOTIFICATIONS.CLOSED);
    doc = null;
    gBrowser.removeCurrentTab();
    finish();
  }
}
