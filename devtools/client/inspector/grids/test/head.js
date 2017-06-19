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
Services.prefs.setBoolPref("devtools.promote.layoutview.showPromoteBar", false);
Services.prefs.setIntPref("devtools.toolbox.footer.height", 350);
registerCleanupFunction(() => {
  Services.prefs.clearUserPref("devtools.layoutview.enabled");
  Services.prefs.clearUserPref("devtools.promote.layoutview.showPromoteBar");
  Services.prefs.clearUserPref("devtools.toolbox.footer.height");
});

const HIGHLIGHTER_TYPE = "CssGridHighlighter";

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
