/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */


function test()
{
  waitForExplicitFinish();

  let doc;
  let node;

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onload() {
    gBrowser.selectedBrowser.removeEventListener("load", onload, true);
    doc = content.document;
    waitForFocus(setupKeyBindingsTest, content);
  }, true);

  content.location = "data:text/html,<h1>foobar</h1>";

  function setupKeyBindingsTest()
  {
    node = doc.querySelector("h1");
    Services.obs.addObserver(highlightNode,
      InspectorUI.INSPECTOR_NOTIFICATIONS.OPENED, false);
    InspectorUI.toggleInspectorUI();
  }

  function highlightNode()
  {
    Services.obs.removeObserver(highlightNode,
      InspectorUI.INSPECTOR_NOTIFICATIONS.OPENED);

    executeSoon(function() {
      Services.obs.addObserver(lockNode,
        InspectorUI.INSPECTOR_NOTIFICATIONS.HIGHLIGHTING, false);

      InspectorUI.inspectNode(node);
    });
  }

  function lockNode()
  {
    Services.obs.removeObserver(lockNode,
      InspectorUI.INSPECTOR_NOTIFICATIONS.HIGHLIGHTING);

    EventUtils.synthesizeKey("VK_RETURN", { });

    executeSoon(isTheNodeLocked);
  }

  function isTheNodeLocked()
  {
    is(InspectorUI.selection, node, "selection matches node");
    ok(!InspectorUI.inspecting, "the node is locked");
    unlockNode();
  }

  function unlockNode() {
    EventUtils.synthesizeKey("VK_RETURN", { });

    executeSoon(isTheNodeUnlocked);
  }

  function isTheNodeUnlocked()
  {
    ok(InspectorUI.inspecting, "the node is unlocked");

    // Let's close the inspector
    Services.obs.addObserver(finishUp,
      InspectorUI.INSPECTOR_NOTIFICATIONS.CLOSED, false);

    EventUtils.synthesizeKey("VK_ESCAPE", {});
    ok(true, "Inspector is closing successfuly");
  }

  function finishUp() {
    Services.obs.removeObserver(finishUp,
                                InspectorUI.INSPECTOR_NOTIFICATIONS.CLOSED);
    doc = node = null;
    gBrowser.removeCurrentTab();
    finish();
  }
}
