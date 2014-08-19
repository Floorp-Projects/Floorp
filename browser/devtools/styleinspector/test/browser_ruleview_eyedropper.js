/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const PAGE_CONTENT = [
  '<style type="text/css">',
  '  body {',
  '    background-color: #ff5;',
  '    padding: 50px',
  '  }',
  '  div {',
  '    width: 100px;',
  '    height: 100px;',
  '    background-color: #f09;',
  '  }',
  '</style>',
  '<body><div></div></body>'
].join("\n");

const ORIGINAL_COLOR = "rgb(255, 0, 153)";  // #f09
const EXPECTED_COLOR = "rgb(255, 255, 85)"; // #ff5

// Test opening the eyedropper from the color picker. Pressing escape
// to close it, and clicking the page to select a color.

let test = asyncTest(function*() {
  yield addTab("data:text/html;charset=utf-8,rule view eyedropper test");
  content.document.body.innerHTML = PAGE_CONTENT;

  let {toolbox, inspector, view} = yield openRuleView();
  yield selectNode("div", inspector);

  let property = getRuleViewProperty(view, "div", "background-color");
  let swatch = property.valueSpan.querySelector(".ruleview-colorswatch");
  ok(swatch, "Color swatch is displayed for the bg-color property");

  let dropper = yield openEyedropper(view, swatch);

  let tooltip = view.tooltips.colorPicker.tooltip;
  ok(tooltip.isHidden(),
     "color picker tooltip is closed after opening eyedropper");

  yield testESC(swatch, dropper);

  dropper = yield openEyedropper(view, swatch);

  ok(dropper, "dropper opened");

  yield testSelect(swatch, dropper);
});

function testESC(swatch, dropper) {
  let deferred = promise.defer();

  dropper.once("destroy", () => {
    let color = swatch.style.backgroundColor;
    is(color, ORIGINAL_COLOR, "swatch didn't change after pressing ESC");

    deferred.resolve();
  });

  inspectPage(dropper, false).then(pressESC);

  return deferred.promise;
}

function testSelect(swatch, dropper) {
  let deferred = promise.defer();

  dropper.once("destroy", () => {
    let color = swatch.style.backgroundColor;
    is(color, EXPECTED_COLOR, "swatch changed colors");

    // the change to the content is done async after rule view change
    executeSoon(() => {
      let element = content.document.querySelector("div");
      is(content.window.getComputedStyle(element).backgroundColor,
         EXPECTED_COLOR,
         "div's color set to body color after dropper");

      deferred.resolve();
    });
  });

  inspectPage(dropper);

  return deferred.promise;
}


/* Helpers */

function openEyedropper(view, swatch) {
  let deferred = promise.defer();

  let tooltip = view.tooltips.colorPicker.tooltip;

  tooltip.once("shown", () => {
    let tooltipDoc = tooltip.content.contentDocument;
    let dropperButton = tooltipDoc.querySelector("#eyedropper-button");

    tooltip.once("eyedropper-opened", (event, dropper) => {
      deferred.resolve(dropper)
    });
    dropperButton.click();
  });

  swatch.click();
  return deferred.promise;
}

function inspectPage(dropper, click=true) {
  let target = content.document.body;
  let win = content.window;

  EventUtils.synthesizeMouse(target, 10, 10, { type: "mousemove" }, win);

  return dropperLoaded(dropper).then(() => {
    EventUtils.synthesizeMouse(target, 20, 20, { type: "mousemove" }, win);
    if (click) {
      EventUtils.synthesizeMouse(target, 20, 20, {}, win);
    }
  });
}

function dropperLoaded(dropper) {
  if (dropper.loaded) {
    return promise.resolve();
  }

  return dropper.once("load");
}

function pressESC() {
  EventUtils.synthesizeKey("VK_ESCAPE", { });
}
