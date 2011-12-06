/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */


function test()
{
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

  function setupObjectInspectionTest()
  {
    objectNode = doc.querySelector("object");
    ok(objectNode, "we have the object node");
    Services.obs.addObserver(runObjectInspectionTest,
      InspectorUI.INSPECTOR_NOTIFICATIONS.OPENED, false);
    InspectorUI.toggleInspectorUI();
  }

  function runObjectInspectionTest()
  {
    Services.obs.removeObserver(runObjectInspectionTest,
      InspectorUI.INSPECTOR_NOTIFICATIONS.OPENED);

    executeSoon(function() {
      Services.obs.addObserver(performTestComparison,
        InspectorUI.INSPECTOR_NOTIFICATIONS.HIGHLIGHTING, false);

      InspectorUI.inspectNode(objectNode);
    });
  }

  function performTestComparison()
  {
    Services.obs.removeObserver(performTestComparison,
      InspectorUI.INSPECTOR_NOTIFICATIONS.HIGHLIGHTING);

    is(InspectorUI.selection, objectNode, "selection matches node");

    Services.obs.addObserver(finishUp,
      InspectorUI.INSPECTOR_NOTIFICATIONS.CLOSED, false);
    InspectorUI.closeInspectorUI();
  }


  function finishUp() {
    Services.obs.removeObserver(finishUp, InspectorUI.INSPECTOR_NOTIFICATIONS.CLOSED);
    doc = objectNode = null;
    gBrowser.removeCurrentTab();
    finish();
  }
}
