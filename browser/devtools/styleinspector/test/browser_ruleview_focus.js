/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that focus doesn't leave the style editor when adding a property
// (bug 719916)

let doc;
let inspector;
let stylePanel;

function openRuleView()
{
  var target = TargetFactory.forTab(gBrowser.selectedTab);
  gDevTools.showToolbox(target, "inspector").then(function(toolbox) {
    inspector = toolbox.getCurrentPanel();
    inspector.sidebar.select("ruleview");

    // Highlight a node.
    let node = content.document.getElementsByTagName("h1")[0];
    inspector.selection.once("new-node", testFocus);
    executeSoon(function() {
      inspector.selection.setNode(doc.body);
    });
  });
}

function testFocus()
{
  let win = inspector.sidebar.getWindowForTab("ruleview");
  let brace = win.document.querySelectorAll(".ruleview-ruleclose")[0];

  waitForEditorFocus(brace.parentNode, function onNewElement(aEditor) {
    aEditor.input.value = "color";
    waitForEditorFocus(brace.parentNode, function onEditingValue(aEditor) {
      // If we actually get this focus we're ok.
      ok(true, "We got focus.");
      aEditor.input.value = "green";

      // If we've retained focus, pressing return will start a new editor.
      // If not, we'll wait here until we time out.
      waitForEditorFocus(brace.parentNode, function onNewEditor(aEditor) {
        aEditor.input.blur();
        finishUp();
      });
      EventUtils.sendKey("return");
    });
    EventUtils.sendKey("return");
  });

  brace.click();
}

function finishUp()
{
  doc = inspector = stylePanel = null;
  gBrowser.removeCurrentTab();
  finish();
}

function test()
{
  waitForExplicitFinish();
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function(evt) {
    gBrowser.selectedBrowser.removeEventListener(evt.type, arguments.callee, true);
    doc = content.document;
    doc.title = "Rule View Test";
    waitForFocus(openRuleView, content);
  }, true);

  content.location = "data:text/html,<h1>Some header text</h1>";
}
