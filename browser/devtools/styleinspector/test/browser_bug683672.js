/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the style inspector works properly

let doc;
let inspector;
let div;
let computedView;

const TEST_URI = "http://example.com/browser/browser/devtools/styleinspector/test/browser_bug683672.html";

let tempScope = {};
let {CssHtmlTree, PropertyView} = devtools.require("devtools/styleinspector/computed-view");
let {console} = Cu.import("resource://gre/modules/devtools/Console.jsm", {});

function test()
{
  waitForExplicitFinish();
  addTab(TEST_URI);
  browser.addEventListener("load", tabLoaded, true);
}

function tabLoaded()
{
  browser.removeEventListener("load", tabLoaded, true);
  doc = content.document;
  openComputedView(selectNode);
}

function selectNode(aInspector, aComputedView)
{
  inspector = aInspector;
  computedView = aComputedView;

  div = content.document.getElementById("test");
  ok(div, "captain, we have the div");

  inspector.selection.setNode(div);
  inspector.once("inspector-updated", runTests);
}

function runTests()
{
  testMatchedSelectors().then(() => {
    info("finishing up");
    finishUp();
  });
}

function testMatchedSelectors()
{
  info("checking selector counts, matched rules and titles");

  is(div, computedView.viewedElement.rawNode(),
      "style inspector node matches the selected node");

  let propertyView = new PropertyView(computedView, "color");
  propertyView.buildMain();
  propertyView.buildSelectorContainer();
  propertyView.matchedExpanded = true;
  return propertyView.refreshMatchedSelectors().then(() => {
    let numMatchedSelectors = propertyView.matchedSelectors.length;

    is(numMatchedSelectors, 6,
        "CssLogic returns the correct number of matched selectors for div");

    is(propertyView.hasMatchedSelectors, true,
        "hasMatchedSelectors returns true");
  }).then(null, (err) => console.error(err));
}

function finishUp()
{
  doc = inspector = div = computedView = null;
  gBrowser.removeCurrentTab();
  finish();
}
