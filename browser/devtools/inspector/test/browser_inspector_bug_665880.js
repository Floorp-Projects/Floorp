/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();
  ignoreAllUncaughtExceptions();

  let doc;
  let objectNode;

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function() {
    gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);
    doc = content.document;
    waitForFocus(setupObjectInspectionTest, content);
  }, true);

  content.location = "data:text/html,<object style='padding: 100px'><p>foobar</p></object>";

  function setupObjectInspectionTest() {
    objectNode = doc.querySelector("object");
    ok(objectNode, "we have the object node");
    openInspector(runObjectInspectionTest);
  }

  function runObjectInspectionTest(inspector) {
    inspector.once("inspector-updated", performTestComparison);
    inspector.selection.setNode(objectNode, "");
  }

  function performTestComparison() {
    is(getActiveInspector().selection.node, objectNode, "selection matches node");
    let target = TargetFactory.forTab(gBrowser.selectedTab);
    executeSoon(function() {
      gDevTools.closeToolbox(target);
      finishUp();
    });
  }

  function finishUp() {
    doc = objectNode = null;
    gBrowser.removeCurrentTab();
    finish();
  }
}
