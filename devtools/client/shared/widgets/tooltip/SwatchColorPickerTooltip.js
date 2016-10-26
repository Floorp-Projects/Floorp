/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Task} = require("devtools/shared/task");
const {colorUtils} = require("devtools/shared/css/color");
const {Spectrum} = require("devtools/client/shared/widgets/Spectrum");
const SwatchBasedEditorTooltip = require("devtools/client/shared/widgets/tooltip/SwatchBasedEditorTooltip");
const {LocalizationHelper} = require("devtools/shared/l10n");
const L10N = new LocalizationHelper("devtools/client/locales/inspector.properties");

const Heritage = require("sdk/core/heritage");

const XHTML_NS = "http://www.w3.org/1999/xhtml";

/**
 * The swatch color picker tooltip class is a specific class meant to be used
 * along with output-parser's generated color swatches.
 * It extends the parent SwatchBasedEditorTooltip class.
 * It just wraps a standard Tooltip and sets its content with an instance of a
 * color picker.
 *
 * @param {Document} document
 *        The document to attach the SwatchColorPickerTooltip. This is either the toolbox
 *        document if the tooltip is a popup tooltip or the panel's document if it is an
 *        inline editor.
 * @param {InspectorPanel} inspector
 *        The inspector panel, needed for the eyedropper.
 */
function SwatchColorPickerTooltip(document, inspector) {
  let stylesheet = "chrome://devtools/content/shared/widgets/spectrum.css";
  SwatchBasedEditorTooltip.call(this, document, stylesheet);

  this.inspector = inspector;

  // Creating a spectrum instance. this.spectrum will always be a promise that
  // resolves to the spectrum instance
  this.spectrum = this.setColorPickerContent([0, 0, 0, 1]);
  this._onSpectrumColorChange = this._onSpectrumColorChange.bind(this);
  this._openEyeDropper = this._openEyeDropper.bind(this);
}

SwatchColorPickerTooltip.prototype = Heritage.extend(SwatchBasedEditorTooltip.prototype, {
  /**
   * Fill the tooltip with a new instance of the spectrum color picker widget
   * initialized with the given color, and return the instance of spectrum
   */
  setColorPickerContent: function (color) {
    let { doc } = this.tooltip;

    let container = doc.createElementNS(XHTML_NS, "div");
    container.id = "spectrum-tooltip";
    let spectrumNode = doc.createElementNS(XHTML_NS, "div");
    spectrumNode.id = "spectrum";
    container.appendChild(spectrumNode);
    let eyedropper = doc.createElementNS(XHTML_NS, "button");
    eyedropper.id = "eyedropper-button";
    eyedropper.className = "devtools-button";
    /* pointerEvents for eyedropper has to be set auto to display tooltip when
     * eyedropper is disabled in non-HTML documents.
     */
    eyedropper.style.pointerEvents = "auto";
    container.appendChild(eyedropper);

    this.tooltip.setContent(container, { width: 218, height: 224 });

    let spectrum = new Spectrum(spectrumNode, color);

    // Wait for the tooltip to be shown before calling spectrum.show
    // as it expect to be visible in order to compute DOM element sizes.
    this.tooltip.once("shown", () => {
      spectrum.show();
    });

    return spectrum;
  },

  /**
   * Overriding the SwatchBasedEditorTooltip.show function to set spectrum's
   * color.
   */
  show: Task.async(function* () {
    // Call then parent class' show function
    yield SwatchBasedEditorTooltip.prototype.show.call(this);
    // Then set spectrum's color and listen to color changes to preview them
    if (this.activeSwatch) {
      this.currentSwatchColor = this.activeSwatch.nextSibling;
      this._originalColor = this.currentSwatchColor.textContent;
      let color = this.activeSwatch.style.backgroundColor;
      this.spectrum.off("changed", this._onSpectrumColorChange);
      this.spectrum.rgb = this._colorToRgba(color);
      this.spectrum.on("changed", this._onSpectrumColorChange);
      this.spectrum.updateUI();
    }

    let {target} = this.inspector;
    target.actorHasMethod("inspector", "pickColorFromPage").then(value => {
      let tooltipDoc = this.tooltip.doc;
      let eyeButton = tooltipDoc.querySelector("#eyedropper-button");
      if (value && this.inspector.selection.nodeFront.isInHTMLDocument) {
        eyeButton.disabled = false;
        eyeButton.removeAttribute("title");
        eyeButton.addEventListener("click", this._openEyeDropper);
      } else {
        eyeButton.disabled = true;
        eyeButton.title = L10N.getStr("eyedropper.disabled.title");
      }
      this.emit("ready");
    }, e => console.error(e));
  }),

  _onSpectrumColorChange: function (event, rgba, cssColor) {
    this._selectColor(cssColor);
  },

  _selectColor: function (color) {
    if (this.activeSwatch) {
      this.activeSwatch.style.backgroundColor = color;
      this.activeSwatch.parentNode.dataset.color = color;

      color = this._toDefaultType(color);
      this.currentSwatchColor.textContent = color;
      this.preview(color);

      if (this.eyedropperOpen) {
        this.commit();
      }
    }
  },

  _openEyeDropper: function () {
    let {inspector, toolbox, telemetry} = this.inspector;
    telemetry.toolOpened("pickereyedropper");
    inspector.pickColorFromPage(toolbox, {copyOnSelect: false}).then(() => {
      this.eyedropperOpen = true;

      // close the colorpicker tooltip so that only the eyedropper is open.
      this.hide();

      this.tooltip.emit("eyedropper-opened");
    }, e => console.error(e));

    inspector.once("color-picked", color => {
      toolbox.win.focus();
      this._selectColor(color);
      this._onEyeDropperDone();
    });

    inspector.once("color-pick-canceled", () => {
      this._onEyeDropperDone();
    });
  },

  _onEyeDropperDone: function () {
    this.eyedropperOpen = false;
    this.activeSwatch = null;
  },

  _colorToRgba: function (color) {
    color = new colorUtils.CssColor(color);
    let rgba = color._getRGBATuple();
    return [rgba.r, rgba.g, rgba.b, rgba.a];
  },

  _toDefaultType: function (color) {
    let colorObj = new colorUtils.CssColor(color);
    colorObj.setAuthoredUnitFromColor(this._originalColor);
    return colorObj.toString();
  },

  destroy: function () {
    SwatchBasedEditorTooltip.prototype.destroy.call(this);
    this.inspector = null;
    this.currentSwatchColor = null;
    this.spectrum.off("changed", this._onSpectrumColorChange);
    this.spectrum.destroy();
  }
});

module.exports = SwatchColorPickerTooltip;
