/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// So we can test collecting telemetry on the eyedropper
let oldCanRecord = Services.telemetry.canRecordExtended;
Services.telemetry.canRecordExtended = true;
registerCleanupFunction(function () {
  Services.telemetry.canRecordExtended = oldCanRecord;
});
const HISTOGRAM_ID = "DEVTOOLS_PICKER_EYEDROPPER_OPENED_BOOLEAN";
const FLAG_HISTOGRAM_ID = "DEVTOOLS_PICKER_EYEDROPPER_OPENED_PER_USER_FLAG";
const EXPECTED_TELEMETRY = {
  "DEVTOOLS_PICKER_EYEDROPPER_OPENED_BOOLEAN": 2,
  "DEVTOOLS_PICKER_EYEDROPPER_OPENED_PER_USER_FLAG": 1
}

const PAGE_CONTENT = [
  '<style type="text/css">',
  '  body {',
  '    background-color: white;',
  '    padding: 0px',
  '  }',
  '',
  '  #div1 {',
  '    background-color: #ff5;',
  '    width: 20px;',
  '    height: 20px;',
  '  }',
  '',
  '  #div2 {',
  '    margin-left: 20px;',
  '    width: 20px;',
  '    height: 20px;',
  '    background-color: #f09;',
  '  }',
  '</style>',
  '<body><div id="div1"></div><div id="div2"></div></body>'
].join("\n");

const ORIGINAL_COLOR = "rgb(255, 0, 153)";  // #f09
const EXPECTED_COLOR = "rgb(255, 255, 85)"; // #ff5

// Test opening the eyedropper from the color picker. Pressing escape
// to close it, and clicking the page to select a color.

add_task(function*() {
  // clear telemetry so we can get accurate counts
  clearTelemetry();

  yield addTab("data:text/html;charset=utf-8,rule view eyedropper test");
  content.document.body.innerHTML = PAGE_CONTENT;

  let {toolbox, inspector, view} = yield openRuleView();
  yield selectNode("#div2", inspector);

  let property = getRuleViewProperty(view, "#div2", "background-color");
  let swatch = property.valueSpan.querySelector(".ruleview-colorswatch");
  ok(swatch, "Color swatch is displayed for the bg-color property");

  let dropper = yield openEyedropper(view, swatch);

  let tooltip = view.tooltips.colorPicker.tooltip;
  ok(tooltip.isHidden(),
     "color picker tooltip is closed after opening eyedropper");

  yield testESC(swatch, dropper);

  dropper = yield openEyedropper(view, swatch);

  ok(dropper, "dropper opened");

  yield testSelect(view, swatch, dropper);

  checkTelemetry();
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

function* testSelect(view, swatch, dropper) {
  let onDestroyed = dropper.once("destroy");
  // the change to the content is done async after rule view change
  let onRuleViewChanged = view.once("ruleview-changed");

  inspectPage(dropper);

  yield onDestroyed;
  yield onRuleViewChanged;

  let color = swatch.style.backgroundColor;
  is(color, EXPECTED_COLOR, "swatch changed colors");

  let element = content.document.querySelector("div");
  is(content.window.getComputedStyle(element).backgroundColor,
     EXPECTED_COLOR,
     "div's color set to body color after dropper");
}

function clearTelemetry() {
  for (let histogramId in EXPECTED_TELEMETRY) {
    let histogram = Services.telemetry.getHistogramById(histogramId);
    histogram.clear();
  }
}

function checkTelemetry() {
  for (let histogramId in EXPECTED_TELEMETRY) {
    let expected = EXPECTED_TELEMETRY[histogramId];
    let histogram = Services.telemetry.getHistogramById(histogramId);
    let snapshot = histogram.snapshot();

    is (snapshot.counts[1], expected,
        "eyedropper telemetry value correct for " + histogramId);
  }
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
  let target = document.documentElement;
  let win = window;

  // get location of the content, offset from browser window
  let box = gBrowser.selectedBrowser.getBoundingClientRect();
  let x = box.left + 1;
  let y = box.top + 1;

  return dropperStarted(dropper).then(() => {
    EventUtils.synthesizeMouse(target, x, y, { type: "mousemove" }, win);

    return dropperLoaded(dropper).then(() => {
      EventUtils.synthesizeMouse(target, x + 10, y + 10, { type: "mousemove" }, win);

      if (click) {
        EventUtils.synthesizeMouse(target, x + 10, y + 10, {}, win);
      }
    });
  });
}

function dropperStarted(dropper) {
  if (dropper.isStarted) {
    return promise.resolve();
  }
  return dropper.once("started");
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
