/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

let {gDevTools} = Cu.import("resource:///modules/devtools/gDevTools.jsm", {});

let contentDoc;
let contentWin;
let inspector;
let ruleView;
let swatch;

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

function test() {
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function(evt) {
    gBrowser.selectedBrowser.removeEventListener(evt.type, arguments.callee, true);
    contentDoc = content.document;
    contentWin = contentDoc.defaultView;
    waitForFocus(createDocument, content);
  }, true);

  content.location = "data:text/html,rule view color picker tooltip test";
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
  let element = contentDoc.querySelector("div");
  inspector.selection.setNode(element, "test");
  inspector.once("inspector-updated", testColorSwatchIsDisplayed);
}

function testColorSwatchIsDisplayed() {
  swatch = getRuleViewProperty("background-color").valueSpan
               .querySelector(".ruleview-colorswatch");
  ok(swatch, "Color swatch is displayed for the bg-color property");

  openEyedropper(testESC);
}

function testESC(event, dropper) {
  dropper.once("destroy", () => {
    let color = ruleView.colorPicker.activeSwatch.style.backgroundColor;
    is(color, ORIGINAL_COLOR, "swatch didn't change after pressing ESC");

    openEyedropper(testSelect);
  });

  inspectPage(dropper, false).then(pressESC);
}

function testSelect(event, dropper) {
  let tooltip = ruleView.colorPicker.tooltip;
  ok(tooltip.isHidden(),
     "color picker tooltip is closed after opening eyedropper");

  dropper.once("destroy", () => {
    let color = ruleView.colorPicker.activeSwatch.style.backgroundColor;
    is(color, EXPECTED_COLOR, "swatch changed colors");

    // the change to the content is done async after rule view change
    executeSoon(() => {
      let element = contentDoc.querySelector("div");
      is(contentWin.getComputedStyle(element).backgroundColor,
         EXPECTED_COLOR,
         "div's color set to body color after dropper");

      endTests();
    });
  });

  inspectPage(dropper);
}

function endTests() {
  executeSoon(function() {
    gDevTools.once("toolbox-destroyed", () => {
      contentDoc = contentWin = inspector = ruleView = swatch = null;
      gBrowser.removeCurrentTab();
      finish();
    });
    inspector._toolbox.destroy();
  });
}

/* Helpers */

function openEyedropper(callback) {
  let tooltip = ruleView.colorPicker.tooltip;

  tooltip.once("shown", () => {
    let tooltipDoc = tooltip.content.contentDocument;
    let dropperButton = tooltipDoc.querySelector("#eyedropper-button");

    tooltip.once("eyedropper-opened", callback);
    dropperButton.click();
  });

  swatch.click();
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

function inspectPage(dropper, click=true) {
  let target = contentDoc.body;
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

  let deferred = promise.defer();
  dropper.once("load", deferred.resolve);

  return deferred.promise;
}

function pressESC() {
  EventUtils.synthesizeKey("VK_ESCAPE", { });
}
