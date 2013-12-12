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
  '    width: 500px;',
  '    height: 300px;',
  '    background: red;',
  '    transform: skew(16deg);',
  '  }',
  '  .test-element {',
  '    transform-origin: top left;',
  '    transform: rotate(45deg);',
  '  }',
  '  div {',
  '    transform: scaleX(1.5);',
  '    transform-origin: bottom right;',
  '  }',
  '  [attr] {',
  '  }',
  '</style>',
  '<div id="testElement" class="test-element" attr="value">transformed element</div>'
].join("\n");

function test() {
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function(evt) {
    gBrowser.selectedBrowser.removeEventListener(evt.type, arguments.callee, true);
    contentDoc = content.document;
    waitForFocus(createDocument, content);
  }, true);

  content.location = "data:text/html,rule view css transform tooltip test";
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
  inspector.once("inspector-updated", testTransformTooltipOnIDSelector);
}

function endTests() {
  contentDoc = inspector = ruleView = computedView = null;
  gBrowser.removeCurrentTab();
  finish();
}

function testTransformTooltipOnIDSelector() {
  info("Testing that a transform tooltip appears on the #ID rule");

  let panel = ruleView.previewTooltip.panel;
  ok(panel, "The XUL panel exists for the rule-view preview tooltips");

  let {valueSpan} = getRuleViewProperty("#testElement", "transform");
  assertTooltipShownOn(ruleView.previewTooltip, valueSpan, () => {
    // The transform preview is canvas, so there's not much we can test, so for
    // now, let's just be happy with the fact that the tooltips is shown!
    ok(true, "Tooltip shown on the transform property of the #ID rule");
    ruleView.previewTooltip.hide();
    executeSoon(testTransformTooltipOnClassSelector);
  });
}

function testTransformTooltipOnClassSelector() {
  info("Testing that a transform tooltip appears on the .class rule");

  let {valueSpan} = getRuleViewProperty(".test-element", "transform");
  assertTooltipShownOn(ruleView.previewTooltip, valueSpan, () => {
    // The transform preview is canvas, so there's not much we can test, so for
    // now, let's just be happy with the fact that the tooltips is shown!
    ok(true, "Tooltip shown on the transform property of the .class rule");
    ruleView.previewTooltip.hide();
    executeSoon(testTransformTooltipOnTagSelector);
  });
}

function testTransformTooltipOnTagSelector() {
  info("Testing that a transform tooltip appears on the tag rule");

  let {valueSpan} = getRuleViewProperty("div", "transform");
  assertTooltipShownOn(ruleView.previewTooltip, valueSpan, () => {
    // The transform preview is canvas, so there's not much we can test, so for
    // now, let's just be happy with the fact that the tooltips is shown!
    ok(true, "Tooltip shown on the transform property of the tag rule");
    ruleView.previewTooltip.hide();
    executeSoon(testTransformTooltipNotShownOnInvalidTransform);
  });
}

function testTransformTooltipNotShownOnInvalidTransform() {
  info("Testing that a transform tooltip does not appear for invalid values");

  let ruleEditor;
  for (let rule of ruleView._elementStyle.rules) {
    if (rule.matchedSelectors[0] === "[attr]") {
      ruleEditor = rule.editor;
    }
  }
  ruleEditor.addProperty("transform", "muchTransform(suchAngle)", "");

  let {valueSpan} = getRuleViewProperty("[attr]", "transform");
  assertTooltipNotShownOn(ruleView.previewTooltip, valueSpan, () => {
    executeSoon(testTransformTooltipOnComputedView);
  });
}

function testTransformTooltipOnComputedView() {
  info("Testing that a transform tooltip appears in the computed view too");

  inspector.sidebar.select("computedview");
  computedView = inspector.sidebar.getWindowForTab("computedview").computedview.view;
  let doc = computedView.styleDocument;

  let panel = computedView.tooltip.panel;
  let {valueSpan} = getComputedViewProperty("transform");

  assertTooltipShownOn(computedView.tooltip, valueSpan, () => {
    // The transform preview is canvas, so there's not much we can test, so for
    // now, let's just be happy with the fact that the tooltips is shown!
    ok(true, "Tooltip shown on the computed transform property");
    computedView.tooltip.hide();
    executeSoon(endTests);
  });
}

function assertTooltipShownOn(tooltip, element, cb) {
  // If there is indeed a show-on-hover on element, the xul panel will be shown
  tooltip.panel.addEventListener("popupshown", function shown() {
    tooltip.panel.removeEventListener("popupshown", shown, true);
    cb();
  }, true);
  tooltip._showOnHover(element);
}

function assertTooltipNotShownOn(tooltip, element, cb) {
  // The only way to make sure the tooltip is not shown is try and show it, wait
  // for a given amount of time, and then check if it's shown or not
  tooltip._showOnHover(element);
  setTimeout(() => {
    ok(!tooltip.isShown(), "The tooltip did not appear on hover of the element");
    cb();
  }, tooltip.defaultShowDelay + 100);
}

function getRule(selectorText) {
  let rule;

  [].forEach.call(ruleView.doc.querySelectorAll(".ruleview-rule"), aRule => {
    let selector = aRule.querySelector(".ruleview-selector-matched");
    if (selector && selector.textContent === selectorText) {
      rule = aRule;
    }
  });

  return rule;
}

function getRuleViewProperty(selectorText, propertyName) {
  let prop;

  let rule = getRule(selectorText);
  if (rule) {
    // Look for the propertyName in that rule element
    [].forEach.call(rule.querySelectorAll(".ruleview-property"), property => {
      let nameSpan = property.querySelector(".ruleview-propertyname");
      let valueSpan = property.querySelector(".ruleview-propertyvalue");

      if (nameSpan.textContent === propertyName) {
        prop = {nameSpan: nameSpan, valueSpan: valueSpan};
      }
    });
  }

  return prop;
}

function getComputedViewProperty(name) {
  let prop;
  [].forEach.call(computedView.styleDocument.querySelectorAll(".property-view"), property => {
    let nameSpan = property.querySelector(".property-name");
    let valueSpan = property.querySelector(".property-value");

    if (nameSpan.textContent === name) {
      prop = {nameSpan: nameSpan, valueSpan: valueSpan};
    }
  });
  return prop;
}
