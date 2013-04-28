/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let tempScope = {};
Cu.import("resource:///modules/devtools/Target.jsm", tempScope);
let TargetFactory = tempScope.TargetFactory;

let DOMUtils = Cc["@mozilla.org/inspector/dom-utils;1"].getService(Ci.inIDOMUtils);

let doc;
let parentDiv, div, div2;
let inspector;
let ruleview;

let pseudo = ":hover";

function test()
{
  waitForExplicitFinish();
  ignoreAllUncaughtExceptions();
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function() {
    gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);
    doc = content.document;
    waitForFocus(createDocument, content);
  }, true);

  content.location = "data:text/html,pseudo-class lock tests";
}

function createDocument()
{
  parentDiv = doc.createElement("div");
  parentDiv.textContent = "parent div";

  div = doc.createElement("div");
  div.textContent = "test div";

  div2 = doc.createElement("div");
  div2.textContent = "test div2";

  let head = doc.getElementsByTagName('head')[0];
  let style = doc.createElement('style');
  let rules = doc.createTextNode('div { color: red; } div:hover { color: blue; }');

  style.appendChild(rules);
  head.appendChild(style);
  parentDiv.appendChild(div);
  parentDiv.appendChild(div2);
  doc.body.appendChild(parentDiv);

  openInspector(selectNode);
}

function selectNode(aInspector)
{
  inspector = aInspector;
  inspector.selection.setNode(div);
  inspector.sidebar.once("ruleview-ready", function() {
    ruleview = inspector.sidebar.getWindowForTab("ruleview").ruleview.view;
    inspector.sidebar.select("ruleview");
    performTests();
  });
}

function performTests()
{
  // toggle the class
  inspector.togglePseudoClass(pseudo);

  testAdded();

  // toggle the lock off
  inspector.togglePseudoClass(pseudo);

  testRemoved();
  testRemovedFromUI();

  // toggle it back on
  inspector.togglePseudoClass(pseudo);

  testNavigate();

  // close the inspector
  finishUp();
}

function testNavigate()
{
  inspector.selection.setNode(parentDiv);

  // make sure it's still on after naving to parent
  is(DOMUtils.hasPseudoClassLock(div, pseudo), true,
       "pseudo-class lock is still applied after inspecting ancestor");

  inspector.selection.setNode(div2);

  // make sure it's removed after naving to a non-hierarchy node
  is(DOMUtils.hasPseudoClassLock(div, pseudo), false,
       "pseudo-class lock is removed after inspecting sibling node");

  // toggle it back on
  inspector.selection.setNode(div);
  inspector.togglePseudoClass(pseudo);
}

function testAdded()
{
  // lock is applied to it and ancestors
  let node = div;
  do {
    is(DOMUtils.hasPseudoClassLock(node, pseudo), true,
       "pseudo-class lock has been applied");
    node = node.parentNode;
  } while (node.parentNode)

  // infobar selector contains pseudo-class
  let pseudoClassesBox = getActiveInspector().highlighter.nodeInfo.pseudoClassesBox;
  is(pseudoClassesBox.textContent, pseudo, "pseudo-class in infobar selector");

  // ruleview contains pseudo-class rule
  is(ruleview.element.children.length, 3,
     "rule view is showing 3 rules for pseudo-class locked div");

  is(ruleview.element.children[1]._ruleEditor.rule.selectorText,
     "div:hover", "rule view is showing " + pseudo + " rule");
}

function testRemoved()
{
  // lock removed from node and ancestors
  let node = div;
  do {
    is(DOMUtils.hasPseudoClassLock(node, pseudo), false,
       "pseudo-class lock has been removed");
    node = node.parentNode;
  } while (node.parentNode)
}

function testRemovedFromUI()
{
  // infobar selector doesn't contain pseudo-class
  let pseudoClassesBox = getActiveInspector().highlighter.nodeInfo.pseudoClassesBox;
  is(pseudoClassesBox.textContent, "", "pseudo-class removed from infobar selector");

  // ruleview no longer contains pseudo-class rule
  is(ruleview.element.children.length, 2,
     "rule view is showing 2 rules after removing lock");
}

function finishUp()
{
  gDevTools.once("toolbox-destroyed", function() {
    testRemoved();
    inspector = ruleview = null;
    doc = div = null;
    gBrowser.removeCurrentTab();
    finish();
  });

  let target = TargetFactory.forTab(gBrowser.selectedTab);
  let toolbox = gDevTools.getToolbox(target);
  toolbox.destroy();
}
