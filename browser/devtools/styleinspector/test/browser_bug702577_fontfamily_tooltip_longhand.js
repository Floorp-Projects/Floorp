/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

let contentDoc;
let inspector;
let ruleView;
let computedView;

const PAGE_CONTENT = [
  '<style type="text/css">',
  '  #testElement {',
  '    font-family: cursive;',
  '    color: #333;',
  '    padding-left: 70px;',
  '  }',
  '</style>',
  '<div id="testElement">test element</div>'
].join("\n");

function test() {
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function(evt) {
    gBrowser.selectedBrowser.removeEventListener(evt.type, arguments.callee, true);
    contentDoc = content.document;
    waitForFocus(createDocument, content);
  }, true);

  content.location = "data:text/html;charset=utf-8,font family longhand tooltip test";
}

function createDocument() {
  contentDoc.body.innerHTML = PAGE_CONTENT;

  openRuleView((aInspector, aRuleView) => {
    inspector = aInspector;
    ruleView = aRuleView;
    startTests();
  });
}

function startTests() {
  inspector.selection.setNode(contentDoc.querySelector("#testElement"));
  inspector.once("inspector-updated", testRuleView);
}

function endTests() {
  contentDoc = inspector = ruleView = computedView = null;
  gBrowser.removeCurrentTab();
  finish();
}

function testRuleView() {
  Task.spawn(function*() {
    info("Testing font-family tooltips in the rule view");

    let panel = ruleView.previewTooltip.panel;

    // Check that the rule view has a tooltip and that a XUL panel has been created
    ok(ruleView.previewTooltip, "Tooltip instance exists");
    ok(panel, "XUL panel exists");

    // Get the font family property inside the rule view
    let {valueSpan} = getRuleViewProperty("font-family");

    // And verify that the tooltip gets shown on this property
    assertTooltipShownOn(ruleView.previewTooltip, valueSpan);

    let description = panel.getElementsByTagName("description")[0];
    is(description.style.fontFamily, "cursive", "Tooltips contains correct font-family style");
  }).then(testComputedView);
}

function testComputedView() {
  Task.spawn(function*() {
    info("Testing font-family tooltips in the computed view");

    inspector.sidebar.select("computedview");
    computedView = inspector.sidebar.getWindowForTab("computedview").computedview.view;
    let doc = computedView.styleDocument;

    let panel = computedView.tooltip.panel;
    let {valueSpan} = getComputedViewProperty("font-family");

    assertTooltipShownOn(computedView.tooltip, valueSpan);

    let description = panel.getElementsByTagName("description")[0];
    is(description.style.fontFamily, "cursive", "Tooltips contains correct font-family style");
  }).then(endTests);
}

function getRuleViewProperty(name) {
  let prop = null;
  [].forEach.call(ruleView.doc.querySelectorAll(".ruleview-property"), property => {
    let nameSpan = property.querySelector(".ruleview-propertyname");
    let valueSpan = property.querySelector(".ruleview-propertyvalue");

    if (nameSpan.textContent === name) {
      prop = {nameSpan: nameSpan, valueSpan: valueSpan};
    }
  });
  return prop;
}

function getComputedViewProperty(name) {
  let prop = null;
  [].forEach.call(computedView.styleDocument.querySelectorAll(".property-view"), property => {
    let nameSpan = property.querySelector(".property-name");
    let valueSpan = property.querySelector(".property-value");

    if (nameSpan.textContent === name) {
      prop = {nameSpan: nameSpan, valueSpan: valueSpan};
    }
  });
  return prop;
}
