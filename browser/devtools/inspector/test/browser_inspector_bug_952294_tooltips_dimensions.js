/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

let contentDoc;
let inspector;
let ruleView;
let markupView;

const PAGE_CONTENT = [
  '<style type="text/css">',
  '  div {',
  '    width: 300px;height: 300px;border-radius: 50%;',
  '    transform: skew(45deg);',
  '    background: red url(chrome://global/skin/icons/warning-64.png);',
  '  }',
  '</style>',
  '<div></div>'
].join("\n");

function test() {
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onload(evt) {
    gBrowser.selectedBrowser.removeEventListener("load", onload, true);
    contentDoc = content.document;
    waitForFocus(createDocument, content);
  }, true);

  content.location = "data:text/html,tooltip dimension test";
}

function createDocument() {
  contentDoc.body.innerHTML = PAGE_CONTENT;

  openInspector(aInspector => {
    inspector = aInspector;
    markupView = inspector.markup;

    waitForView("ruleview", () => {
      ruleView = inspector.sidebar.getWindowForTab("ruleview").ruleview.view;
      inspector.sidebar.select("ruleview");
      startTests();
    });
  });
}

function endTests() {
  contentDoc = inspector = ruleView = markupView = null;
  gBrowser.removeCurrentTab();
  finish();
}

function startTests() {
  Task.spawn(function() {
    yield selectDiv();
    yield testTransformDimension();
    yield testImageDimension();
    yield testPickerDimension();
    endTests();
  }).then(null, Cu.reportError);
}

function selectDiv() {
  let deferred = promise.defer();

  inspector.selection.setNode(contentDoc.querySelector("div"));
  inspector.once("inspector-updated", () => {
    deferred.resolve();
  });

  return deferred.promise;
}

function testTransformDimension() {
  return Task.spawn(function*() {
    let tooltip = ruleView.previewTooltip;
    let panel = tooltip.panel;

    info("Testing css transform tooltip dimensions");
    let {valueSpan} = getRuleViewProperty("transform");

    yield assertTooltipShownOn(tooltip, valueSpan);

    // Let's not test for a specific size, but instead let's make sure it's at
    // least as big as the preview canvas
    let canvas = panel.querySelector("canvas");
    let w = canvas.width;
    let h = canvas.height;
    let panelRect = panel.getBoundingClientRect();

    ok(panelRect.width >= w, "The panel is wide enough to show the canvas");
    ok(panelRect.height >= h, "The panel is high enough to show the canvas");

    let onHidden = tooltip.once("hidden");
    tooltip.hide();
    yield onHidden;
  });
}

function testImageDimension() {
  return Task.spawn(function*() {
    info("Testing background-image tooltip dimensions");

    let tooltip = ruleView.previewTooltip;
    let panel = tooltip.panel;

    let {valueSpan} = getRuleViewProperty("background");
    let uriSpan = valueSpan.querySelector(".theme-link");

    yield assertTooltipShownOn(tooltip, uriSpan);

    // Let's not test for a specific size, but instead let's make sure it's at
    // least as big as the image
    let imageRect = panel.querySelector("image").getBoundingClientRect();
    let panelRect = panel.getBoundingClientRect();

    ok(panelRect.width >= imageRect.width,
      "The panel is wide enough to show the image");
    ok(panelRect.height >= imageRect.height,
      "The panel is high enough to show the image");

    let onHidden = tooltip.once("hidden");
    tooltip.hide();
    yield onHidden;
  });
}

function testPickerDimension() {
  return Task.spawn(function*() {
    info("Testing color-picker tooltip dimensions");

    let {valueSpan} = getRuleViewProperty("background");
    let swatch = valueSpan.querySelector(".ruleview-colorswatch");
    let cPicker = ruleView.colorPicker;

    let onShown = cPicker.tooltip.once("shown");
    swatch.click();
    yield onShown;

    // The colorpicker spectrum's iframe has a fixed width height, so let's
    // make sure the tooltip is at least as big as that
    let w = cPicker.tooltip.panel.querySelector("iframe").width;
    let h = cPicker.tooltip.panel.querySelector("iframe").height;
    let panelRect = cPicker.tooltip.panel.getBoundingClientRect();

    ok(panelRect.width >= w, "The panel is wide enough to show the picker");
    ok(panelRect.height >= h, "The panel is high enough to show the picker");

    let onHidden = cPicker.tooltip.once("hidden");
    cPicker.hide();
    yield onHidden;
  });
}

/**
 * @return a promise that resolves when the tooltip is shown
 */
function assertTooltipShownOn(tooltip, element) {
  return Task.spawn(function*() {
    let isTarget = yield isHoverTooltipTarget(tooltip, element);
    ok(isTarget, "The element is a tooltip target and content has been inserted");

    info("Showing the tooltip now that content has been inserted by isValidHoverTarget");
    let onShown = tooltip.once("shown");
    tooltip.show();
    yield onShown;
  });
}

/**
 * Given a tooltip object instance (see Tooltip.js), checks if it is set to
 * toggle and hover and if so, checks if the given target is a valid hover target.
 * This won't actually show the tooltip (the less we interact with XUL panels
 * during test runs, the better).
 * @return a promise that resolves when the answer is known. Also, this will
 * delete to a function in the rule-view which will insert content into the
 * tooltip
 */
function isHoverTooltipTarget(tooltip, target) {
  if (!tooltip._basedNode || !tooltip.panel) {
    return promise.reject(new Error("The tooltip passed isn't set to toggle on hover or is not a tooltip"));
  }
  // The tooltip delegates to a user defined cb that inserts content in the tooltip
  // when calling isValidHoverTarget
  return tooltip.isValidHoverTarget(target);
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
