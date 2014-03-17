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
  let deferred = promise.defer();
  info("Testing css transform tooltip dimensions");

  let {valueSpan} = getRuleViewProperty("transform");
  showTooltipOn(ruleView.previewTooltip, valueSpan, () => {
    let panel = ruleView.previewTooltip.panel;

    // Let's not test for a specific size, but instead let's make sure it's at
    // least as big as the preview canvas
    let canvas = panel.querySelector("canvas");
    let w = canvas.width;
    let h = canvas.height;
    let panelRect = panel.getBoundingClientRect();

    ok(panelRect.width >= w, "The panel is wide enough to show the canvas");
    ok(panelRect.height >= h, "The panel is high enough to show the canvas");

    ruleView.previewTooltip.hide();
    deferred.resolve();
  });

  return deferred.promise;
}

function testImageDimension() {
  let deferred = promise.defer();
  info("Testing background-image tooltip dimensions");

  let {valueSpan} = getRuleViewProperty("background");
  let uriSpan = valueSpan.querySelector(".theme-link");

  showTooltipOn(ruleView.previewTooltip, uriSpan, () => {
    let panel = ruleView.previewTooltip.panel;

    // Let's not test for a specific size, but instead let's make sure it's at
    // least as big as the image
    let imageRect = panel.querySelector("image").getBoundingClientRect();
    let panelRect = panel.getBoundingClientRect();

    ok(panelRect.width >= imageRect.width,
      "The panel is wide enough to show the image");
    ok(panelRect.height >= imageRect.height,
      "The panel is high enough to show the image");

    ruleView.previewTooltip.hide();
    deferred.resolve();
  });

  return deferred.promise;
}

function testPickerDimension() {
  let deferred = promise.defer();
  info("Testing color-picker tooltip dimensions");

  let {valueSpan} = getRuleViewProperty("background");
  let swatch = valueSpan.querySelector(".ruleview-colorswatch");
  let cPicker = ruleView.colorPicker;

  cPicker.tooltip.once("shown", () => {
    // The colorpicker spectrum's iframe has a fixed width height, so let's
    // make sure the tooltip is at least as big as that
    let w = cPicker.tooltip.panel.querySelector("iframe").width;
    let h = cPicker.tooltip.panel.querySelector("iframe").height;
    let panelRect = cPicker.tooltip.panel.getBoundingClientRect();

    ok(panelRect.width >= w, "The panel is wide enough to show the picker");
    ok(panelRect.height >= h, "The panel is high enough to show the picker");

    cPicker.hide();
    deferred.resolve();
  });
  swatch.click();

  return deferred.promise;
}

function showTooltipOn(tooltip, element, cb) {
  // If there is indeed a show-on-hover on element, the xul panel will be shown
  tooltip.panel.addEventListener("popupshown", function shown() {
    tooltip.panel.removeEventListener("popupshown", shown, true);
    cb();
  }, true);
  tooltip._showOnHover(element);
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
