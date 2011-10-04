/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */


// Tests that the keybindings for highlighting different elements work as
// intended.

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

  content.location = "data:text/html,<html><head><title>Test for the " +
                     "highlighter keybindings</title></head><body><h1>Hello" +
                     "</h1><p><strong>Greetings, earthlings!</strong> I come" +
                     " in peace.</body></html>";

  function setupKeyBindingsTest()
  {
    Services.obs.addObserver(findAndHighlightNode,
                             InspectorUI.INSPECTOR_NOTIFICATIONS.OPENED,
                             false);
    InspectorUI.toggleInspectorUI();
  }

  function findAndHighlightNode()
  {
    Services.obs.removeObserver(findAndHighlightNode,
                                InspectorUI.INSPECTOR_NOTIFICATIONS.OPENED);

    executeSoon(function() {
      Services.obs.addObserver(highlightBodyNode,
                               InspectorUI.INSPECTOR_NOTIFICATIONS.HIGHLIGHTING,
                               false);
      // Test that navigating around without a selected node gets us to the
      // body element.
      node = doc.querySelector("body");
      EventUtils.synthesizeKey("VK_RIGHT", { });
    });
  }

  function highlightBodyNode()
  {
    Services.obs.removeObserver(highlightBodyNode,
                                InspectorUI.INSPECTOR_NOTIFICATIONS.HIGHLIGHTING);
    is(InspectorUI.selection, node, "selected body element");

    executeSoon(function() {
      Services.obs.addObserver(highlightHeaderNode,
                               InspectorUI.INSPECTOR_NOTIFICATIONS.HIGHLIGHTING,
                               false);
      // Test that moving to the child works.
      node = doc.querySelector("h1");
      EventUtils.synthesizeKey("VK_RIGHT", { });
    });
  }

  function highlightHeaderNode()
  {
    Services.obs.removeObserver(highlightHeaderNode,
                                InspectorUI.INSPECTOR_NOTIFICATIONS.HIGHLIGHTING);
    is(InspectorUI.selection, node, "selected h1 element");

    executeSoon(function() {
      Services.obs.addObserver(highlightParagraphNode,
                               InspectorUI.INSPECTOR_NOTIFICATIONS.HIGHLIGHTING,
                               false);
      // Test that moving to the next sibling works.
      node = doc.querySelector("p");
      EventUtils.synthesizeKey("VK_DOWN", { });
    });
  }

  function highlightParagraphNode()
  {
    Services.obs.removeObserver(highlightParagraphNode,
                                InspectorUI.INSPECTOR_NOTIFICATIONS.HIGHLIGHTING);
    is(InspectorUI.selection, node, "selected p element");

    executeSoon(function() {
      Services.obs.addObserver(highlightHeaderNodeAgain,
                               InspectorUI.INSPECTOR_NOTIFICATIONS.HIGHLIGHTING,
                               false);
      // Test that moving to the previous sibling works.
      node = doc.querySelector("h1");
      EventUtils.synthesizeKey("VK_UP", { });
    });
  }

  function highlightHeaderNodeAgain()
  {
    Services.obs.removeObserver(highlightHeaderNodeAgain,
                                InspectorUI.INSPECTOR_NOTIFICATIONS.HIGHLIGHTING);
    is(InspectorUI.selection, node, "selected h1 element");

    executeSoon(function() {
      Services.obs.addObserver(highlightParentNode,
                               InspectorUI.INSPECTOR_NOTIFICATIONS.HIGHLIGHTING,
                               false);
      // Test that moving to the parent works.
      node = doc.querySelector("body");
      EventUtils.synthesizeKey("VK_LEFT", { });
    });
  }

  function highlightParentNode()
  {
    Services.obs.removeObserver(highlightParentNode,
                                InspectorUI.INSPECTOR_NOTIFICATIONS.HIGHLIGHTING);
    is(InspectorUI.selection, node, "selected body element");

    // Test that locking works.
    EventUtils.synthesizeKey("VK_RETURN", { });
    executeSoon(isTheNodeLocked);
  }

  function isTheNodeLocked()
  {
    ok(!InspectorUI.inspecting, "the node is locked");
    Services.obs.addObserver(finishUp,
                             InspectorUI.INSPECTOR_NOTIFICATIONS.CLOSED,
                             false);
    InspectorUI.closeInspectorUI();
  }

  function finishUp() {
    Services.obs.removeObserver(finishUp,
                                InspectorUI.INSPECTOR_NOTIFICATIONS.CLOSED);
    doc = node = null;
    gBrowser.removeCurrentTab();
    finish();
  }
}
