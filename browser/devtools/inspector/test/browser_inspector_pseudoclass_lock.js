/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

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
  inspector.sidebar.once("ruleview-ready", function() {
    ruleview = inspector.sidebar.getWindowForTab("ruleview").ruleview.view;
    inspector.sidebar.select("ruleview");
    inspector.selection.setNode(div);
    inspector.once("inspector-updated", performTests);
  });
}

function performTests()
{
  // toggle the class
  inspector.togglePseudoClass(pseudo);

  // Wait for the "pseudoclass" event so we know the
  // inspector has been told of the pseudoclass lock change.
  inspector.selection.once("pseudoclass", () => {
    inspector.once("rule-view-refreshed", () => {
      testAdded(() => {
        // Change the pseudo class and give the rule view time to update.
        inspector.togglePseudoClass(pseudo);
        inspector.selection.once("pseudoclass", () => {
          inspector.once("rule-view-refreshed", () => {
            testRemoved();
            testRemovedFromUI(() => {
              // toggle it back on
              inspector.togglePseudoClass(pseudo);
              inspector.selection.once("pseudoclass", () => {
                inspector.once("rule-view-refreshed", () => {
                  testNavigate(() => {
                    // close the inspector
                    finishUp();
                  });
                });
              });
            });
          });
        });
      });
    });
  });
}

function testNavigate(callback)
{
  inspector.selection.setNode(parentDiv);
  inspector.once("inspector-updated", () => {

    // make sure it's still on after naving to parent
    is(DOMUtils.hasPseudoClassLock(div, pseudo), true,
      "pseudo-class lock is still applied after inspecting ancestor");

    inspector.selection.setNode(div2);
    inspector.selection.once("pseudoclass", () => {
      // make sure it's removed after naving to a non-hierarchy node
      is(DOMUtils.hasPseudoClassLock(div, pseudo), false,
        "pseudo-class lock is removed after inspecting sibling node");

      // toggle it back on
      inspector.selection.setNode(div);
      inspector.once("inspector-updated", () => {
        inspector.togglePseudoClass(pseudo);
        inspector.once("computed-view-refreshed", callback);
      });
    });
  });
}

function showPickerOn(node, cb)
{
  let highlighter = inspector.toolbox.highlighter;
  highlighter.showBoxModel(getNodeFront(node)).then(cb);
}

function testAdded(cb)
{
  // lock is applied to it and ancestors
  let node = div;
  do {
    is(DOMUtils.hasPseudoClassLock(node, pseudo), true,
      "pseudo-class lock has been applied");
    node = node.parentNode;
  } while (node.parentNode)

  // ruleview contains pseudo-class rule
  let rules = ruleview.element.querySelectorAll(".ruleview-rule.theme-separator");
  is(rules.length, 3, "rule view is showing 3 rules for pseudo-class locked div");
  is(rules[1]._ruleEditor.rule.selectorText, "div:hover", "rule view is showing " + pseudo + " rule");

  // Show the highlighter by starting the pick mode and hovering over the div
  showPickerOn(div, () => {
    // infobar selector contains pseudo-class
    let pseudoClassesBox = getHighlighter().querySelector(".highlighter-nodeinfobar-pseudo-classes");
    is(pseudoClassesBox.textContent, pseudo, "pseudo-class in infobar selector");
    cb();
  });
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

function testRemovedFromUI(cb)
{
  // ruleview no longer contains pseudo-class rule
  let rules = ruleview.element.querySelectorAll(".ruleview-rule.theme-separator");
  is(rules.length, 2, "rule view is showing 2 rules after removing lock");

  showPickerOn(div, () => {
    let pseudoClassesBox = getHighlighter().querySelector(".highlighter-nodeinfobar-pseudo-classes");
    is(pseudoClassesBox.textContent, "", "pseudo-class removed from infobar selector");
    cb();
  });
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
