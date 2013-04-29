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
  openInspector(selectNode);
}

function selectNode(aInspector)
{
  inspector = aInspector;

  div = content.document.getElementById("test");
  ok(div, "captain, we have the div");

  inspector.selection.setNode(div);

  inspector.sidebar.once("computedview-ready", function() {
    computedView = getComputedView(inspector);

    inspector.sidebar.select("computedview");
    runTests();
  });
}

function runTests()
{
  testMatchedSelectors();

  info("finishing up");
  finishUp();
}

function testMatchedSelectors()
{
  info("checking selector counts, matched rules and titles");

  is(div, computedView.viewedElement,
      "style inspector node matches the selected node");

  let propertyView = new PropertyView(computedView, "color");
  let numMatchedSelectors = propertyView.propertyInfo.matchedSelectors.length;

  is(numMatchedSelectors, 6,
      "CssLogic returns the correct number of matched selectors for div");

  is(propertyView.hasMatchedSelectors, true,
      "hasMatchedSelectors returns true");
}

function finishUp()
{
  doc = inspector = div = computedView = null;
  gBrowser.removeCurrentTab();
  finish();
}
