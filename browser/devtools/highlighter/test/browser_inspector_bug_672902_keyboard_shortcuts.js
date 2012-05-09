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
      InspectorUI.highlighter.addListener("nodeselected", highlightBodyNode);
      // Test that navigating around without a selected node gets us to the
      // body element.
      node = doc.querySelector("body");
      EventUtils.synthesizeKey("VK_RIGHT", { });
    });
  }

  function highlightBodyNode()
  {
    InspectorUI.highlighter.removeListener("nodeselected", highlightBodyNode);
    is(InspectorUI.selection, node, "selected body element");

    executeSoon(function() {
      InspectorUI.highlighter.addListener("nodeselected", highlightHeaderNode);
      // Test that moving to the child works.
      node = doc.querySelector("h1");
      EventUtils.synthesizeKey("VK_RIGHT", { });
    });
  }

  function highlightHeaderNode()
  {
    InspectorUI.highlighter.removeListener("nodeselected", highlightHeaderNode);
    is(InspectorUI.selection, node, "selected h1 element");

    executeSoon(function() {
      InspectorUI.highlighter.addListener("nodeselected", highlightParagraphNode);
      // Test that moving to the next sibling works.
      node = doc.querySelector("p");
      EventUtils.synthesizeKey("VK_DOWN", { });
    });
  }

  function highlightParagraphNode()
  {
    InspectorUI.highlighter.removeListener("nodeselected", highlightParagraphNode);
    is(InspectorUI.selection, node, "selected p element");

    executeSoon(function() {
      InspectorUI.highlighter.addListener("nodeselected", highlightHeaderNodeAgain);
      // Test that moving to the previous sibling works.
      node = doc.querySelector("h1");
      EventUtils.synthesizeKey("VK_UP", { });
    });
  }

  function highlightHeaderNodeAgain()
  {
    InspectorUI.highlighter.removeListener("nodeselected", highlightHeaderNodeAgain);
    is(InspectorUI.selection, node, "selected h1 element");

    executeSoon(function() {
      InspectorUI.highlighter.addListener("nodeselected", highlightParentNode);
      // Test that moving to the parent works.
      node = doc.querySelector("body");
      EventUtils.synthesizeKey("VK_LEFT", { });
    });
  }

  function highlightParentNode()
  {
    InspectorUI.highlighter.removeListener("nodeselected", highlightParentNode);
    is(InspectorUI.selection, node, "selected body element");

    // Test that locking works.
    synthesizeKeyFromKeyTag("key_inspect");

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
