/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

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
  let target = document.documentElement;
  let win = window;

  // get location of the content, offset from browser window
  let box = gBrowser.selectedTab.linkedBrowser.getBoundingClientRect();
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
