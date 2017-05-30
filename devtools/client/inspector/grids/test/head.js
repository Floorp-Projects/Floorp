/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint no-unused-vars: [2, {"vars": "local"}] */
/* import-globals-from ../../../framework/test/shared-head.js */
/* import-globals-from ../../test/head.js */
"use strict";

// Import the inspector's head.js first (which itself imports shared-head.js).
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/inspector/test/head.js",
  this);

// Load the shared Redux helpers into this compartment.
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/framework/test/shared-redux-head.js",
  this);

Services.prefs.setBoolPref("devtools.layoutview.enabled", true);
Services.prefs.setIntPref("devtools.toolbox.footer.height", 350);
registerCleanupFunction(() => {
  Services.prefs.clearUserPref("devtools.layoutview.enabled");
  Services.prefs.clearUserPref("devtools.toolbox.footer.height");
});

const HIGHLIGHTER_TYPE = "CssGridHighlighter";

/**
 * Open the toolbox, with the inspector tool visible, and the layout view
 * sidebar tab selected to display the box model view with properties.
 *
 * @return {Promise} a promise that resolves when the inspector is ready and the box model
 *         view is visible and ready.
 */
function openLayoutView() {
  return openInspectorSidebarTab("layoutview").then(data => {
    // The actual highligher show/hide methods are mocked in box model tests.
    // The highlighter is tested in devtools/inspector/test.
    function mockHighlighter({highlighter}) {
      highlighter.showBoxModel = function () {
        return promise.resolve();
      };
      highlighter.hideBoxModel = function () {
        return promise.resolve();
      };
    }
    mockHighlighter(data.toolbox);

    return {
      toolbox: data.toolbox,
      inspector: data.inspector,
      gridInspector: data.inspector.gridInspector,
      testActor: data.testActor
    };
  });
}

/**
 * Simulate a color change in a given color picker tooltip.
 *
 * @param  {Spectrum|ColorWidget} colorPicker
 *         The color picker widget.
 * @param  {Array} newRgba
 *         Array of the new rgba values to be set in the color widget.
 */
var simulateColorPickerChange = Task.async(function* (colorPicker, newRgba) {
  info("Getting the spectrum colorpicker object");
  let spectrum = yield colorPicker.spectrum;
  info("Setting the new color");
  spectrum.rgb = newRgba;
  info("Applying the change");
  spectrum.updateUI();
  spectrum.onChange();
});
