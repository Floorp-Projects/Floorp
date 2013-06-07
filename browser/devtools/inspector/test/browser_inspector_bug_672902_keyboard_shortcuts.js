/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */


// Tests that the keybindings for highlighting different elements work as
// intended.

function test()
{
  waitForExplicitFinish();

  let doc;
  let node;
  let inspector;

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
    openInspector(findAndHighlightNode);
  }

  function findAndHighlightNode(aInspector, aToolbox)
  {
    inspector = aInspector;

    // Make sure the body element is selected initially.
    node = doc.querySelector("body");
    inspector.once("inspector-updated", () => {
      is(inspector.selection.node, node, "Body should be selected initially.");
      node = doc.querySelector("h1")
      inspector.once("inspector-updated", highlightHeaderNode);
      let bc = inspector.breadcrumbs;
      bc.nodeHierarchy[bc.currentIndex].button.focus();
      EventUtils.synthesizeKey("VK_RIGHT", { });
    });
  }

  function highlightHeaderNode()
  {
    is(inspector.selection.node, node, "selected h1 element");

    executeSoon(function() {
      inspector.once("inspector-updated", highlightParagraphNode);
      // Test that moving to the next sibling works.
      node = doc.querySelector("p");
      EventUtils.synthesizeKey("VK_DOWN", { });
    });
  }

  function highlightParagraphNode()
  {
    is(inspector.selection.node, node, "selected p element");

    executeSoon(function() {
      inspector.once("inspector-updated", highlightHeaderNodeAgain);
      // Test that moving to the previous sibling works.
      node = doc.querySelector("h1");
      EventUtils.synthesizeKey("VK_UP", { });
    });
  }

  function highlightHeaderNodeAgain()
  {
    is(inspector.selection.node, node, "selected h1 element");

    executeSoon(function() {
      inspector.once("inspector-updated", highlightParentNode);
      // Test that moving to the parent works.
      node = doc.querySelector("body");
      EventUtils.synthesizeKey("VK_LEFT", { });
    });
  }

  function highlightParentNode()
  {
    is(inspector.selection.node, node, "selected body element");
    finishUp();
  }

  function finishUp() {
    doc = node = null;
    gBrowser.removeCurrentTab();
    finish();
  }
}
