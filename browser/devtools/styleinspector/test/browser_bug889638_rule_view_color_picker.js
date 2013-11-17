/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

let {gDevTools} = Cu.import("resource:///modules/devtools/gDevTools.jsm", {});

let contentDoc;
let contentWin;
let inspector;
let ruleView;
let swatches = [];

const PAGE_CONTENT = [
  '<style type="text/css">',
  '  body {',
  '    color: red;',
  '    background-color: #ededed;',
  '    background-image: url(chrome://global/skin/icons/warning-64.png);',
  '    border: 2em solid rgba(120, 120, 120, .5);',
  '  }',
  '</style>',
  'Testing the color picker tooltip!'
].join("\n");

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
  inspector.selection.setNode(contentDoc.body);
  inspector.once("inspector-updated", testColorSwatchesAreDisplayed);
}

function endTests() {
  executeSoon(function() {
    gDevTools.once("toolbox-destroyed", () => {
      contentDoc = contentWin = inspector = ruleView = swatches = null;
      gBrowser.removeCurrentTab();
      finish();
    });
    inspector._toolbox.destroy();
  });
}

function testColorSwatchesAreDisplayed() {
  let cSwatch = getRuleViewProperty("color").valueSpan
    .querySelector(".ruleview-colorswatch");
  ok(cSwatch, "Color swatch is displayed for the color property");

  let bgSwatch = getRuleViewProperty("background-color").valueSpan
    .querySelector(".ruleview-colorswatch");
  ok(bgSwatch, "Color swatch is displayed for the bg-color property");

  let bSwatch = getRuleViewProperty("border").valueSpan
    .querySelector(".ruleview-colorswatch");
  ok(bSwatch, "Color swatch is displayed for the border property");

  swatches = [cSwatch, bgSwatch, bSwatch];

  testColorPickerAppearsOnColorSwatchClick();
}

function testColorPickerAppearsOnColorSwatchClick() {
  let cPicker = ruleView.colorPicker;
  let cPickerPanel = cPicker.tooltip.panel;
  ok(cPickerPanel, "The XUL panel for the color picker exists");

  function clickOnSwatch(index, cb) {
    if (index === swatches.length) {
      return cb();
    }

    let swatch = swatches[index];
    cPicker.tooltip.once("shown", () => {
      ok(true, "The color picker was shown on click of the color swatch");
      ok(!inplaceEditor(swatch.parentNode),
        "The inplace editor wasn't shown as a result of the color swatch click");
      cPicker.hide();
      clickOnSwatch(index + 1, cb);
    });
    swatch.click();
  }

  clickOnSwatch(0, testColorPickerHidesWhenImageTooltipAppears);
}

function testColorPickerHidesWhenImageTooltipAppears() {
  let swatch = swatches[0];
  let bgImageSpan = getRuleViewProperty("background-image").valueSpan;
  let uriSpan = bgImageSpan.querySelector(".theme-link");

  ruleView.colorPicker.tooltip.once("shown", () => {
    info("The color picker is shown, now display an image tooltip to hide it");
    ruleView.previewTooltip._showOnHover(uriSpan);
  });
  ruleView.colorPicker.tooltip.once("hidden", () => {
    ok(true, "The color picker closed when the image preview tooltip appeared");

    executeSoon(() => {
      ruleView.previewTooltip.hide();
      testPressingEscapeRevertsChanges();
    });
  });

  swatch.click();
}

function testPressingEscapeRevertsChanges() {
  let cPicker = ruleView.colorPicker;
  let swatch = swatches[1];

  cPicker.tooltip.once("shown", () => {
    simulateColorChange(cPicker, [0, 0, 0, 1]);

    is(swatch.style.backgroundColor, "rgb(0, 0, 0)",
      "The color swatch's background was updated");
    is(getRuleViewProperty("background-color").valueSpan.textContent, "rgba(0, 0, 0, 1)",
      "The text of the background-color css property was updated");

    // The change to the content element is done async, via the protocol,
    // that's why we need to let it go first
    executeSoon(() => {
      is(contentWin.getComputedStyle(contentDoc.body).backgroundColor,
        "rgb(0, 0, 0)", "The element's background-color was changed");

      cPicker.spectrum.then(spectrum => {
        // ESC out of the color picker
        EventUtils.sendKey("ESCAPE", spectrum.element.ownerDocument.defaultView);
        executeSoon(() => {
          is(contentWin.getComputedStyle(contentDoc.body).backgroundColor,
            "rgb(237, 237, 237)", "The element's background-color was reverted");

          testPressingEnterCommitsChanges();
        });
      });
    });
  });

  swatch.click();
}

function testPressingEnterCommitsChanges() {
  let cPicker = ruleView.colorPicker;
  let swatch = swatches[2]; // That's the border css declaration

  cPicker.tooltip.once("shown", () => {
    simulateColorChange(cPicker, [0, 255, 0, .5]);

    is(swatch.style.backgroundColor, "rgba(0, 255, 0, 0.5)",
      "The color swatch's background was updated");
    is(getRuleViewProperty("border").valueSpan.textContent,
      "2em solid rgba(0, 255, 0, 0.5)",
      "The text of the border css property was updated");

    // The change to the content element is done async, via the protocol,
    // that's why we need to let it go first
    executeSoon(() => {
      is(contentWin.getComputedStyle(contentDoc.body).borderLeftColor,
        "rgba(0, 255, 0, 0.5)", "The element's border was changed");

      cPicker.tooltip.once("hidden", () => {
        is(contentWin.getComputedStyle(contentDoc.body).borderLeftColor,
          "rgba(0, 255, 0, 0.5)", "The element's border was kept after ENTER");
        is(swatch.style.backgroundColor, "rgba(0, 255, 0, 0.5)",
          "The color swatch's background was kept after ENTER");
        is(getRuleViewProperty("border").valueSpan.textContent,
          "2em solid rgba(0, 255, 0, 0.5)",
          "The text of the border css property was kept after ENTER");

        endTests();
      });

      cPicker.spectrum.then(spectrum => {
        EventUtils.sendKey("RETURN", spectrum.element.ownerDocument.defaultView);
      });
    });
  });

  swatch.click();
}

function simulateColorChange(colorPicker, newRgba) {
  // Note that this test isn't concerned with simulating events to test how the
  // spectrum color picker reacts, see browser_spectrum.js for this.
  // This test only cares about the color swatch <-> color picker <-> rule view
  // interactions. That's why there's no event simulations here
  colorPicker.spectrum.then(spectrum => {
    spectrum.rgb = newRgba;
    spectrum.updateUI();
    spectrum.onChange();
  });
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
