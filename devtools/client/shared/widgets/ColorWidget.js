/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This file is a new working copy of Spectrum.js for the purposes of refreshing the color
 * widget. It is hidden behind a pref("devtools.inspector.colorWidget.enabled").
 */

"use strict";

const {Task} = require("devtools/shared/task");
const EventEmitter = require("devtools/shared/event-emitter");
const {colorUtils} = require("devtools/shared/css/color");
const {LocalizationHelper} = require("devtools/shared/l10n");
const L10N = new LocalizationHelper("devtools/client/locales/inspector.properties");

const XHTML_NS = "http://www.w3.org/1999/xhtml";
const SAMPLE_TEXT = "Abc";

/**
 * ColorWidget creates a color picker widget in any container you give it.
 *
 * Simple usage example:
 *
 * const {ColorWidget} = require("devtools/client/shared/widgets/ColorWidget");
 * let s = new ColorWidget(containerElement, [255, 126, 255, 1]);
 * s.on("changed", (event, rgba, color) => {
 *   console.log("rgba(" + rgba[0] + ", " + rgba[1] + ", " + rgba[2] + ", " +
 *     rgba[3] + ")");
 * });
 * s.show();
 * s.destroy();
 *
 * Note that the color picker is hidden by default and you need to call show to
 * make it appear. This 2 stages initialization helps in cases you are creating
 * the color picker in a parent element that hasn't been appended anywhere yet
 * or that is hidden. Calling show() when the parent element is appended and
 * visible will allow the color widget to correctly initialize its various parts.
 *
 * Fires the following events:
 * - changed : When the user changes the current color
 */
function ColorWidget(parentEl, rgb) {
  EventEmitter.decorate(this);

  this.parentEl = parentEl;

  this.onAlphaSliderMove = this.onAlphaSliderMove.bind(this);
  this.onElementClick = this.onElementClick.bind(this);
  this.onDraggerMove = this.onDraggerMove.bind(this);
  this.onHexInputChange = this.onHexInputChange.bind(this);
  this.onHslaInputChange = this.onHslaInputChange.bind(this);
  this.onRgbaInputChange = this.onRgbaInputChange.bind(this);
  this.onSelectValueChange = this.onSelectValueChange.bind(this);
  this.onSliderMove = this.onSliderMove.bind(this);
  this.updateContrast = this.updateContrast.bind(this);

  this.initializeColorWidget();

  if (rgb) {
    this.rgb = rgb;
    this.updateUI();
  }
}

module.exports.ColorWidget = ColorWidget;

/**
 * Calculates the contrast grade and title for the given contrast
 * ratio and background color.
 * @param {Number} contrastRatio Contrast ratio to calculate grade.
 * @param {String} backgroundColor A string of the form `rgba(r, g, b, a)`
 * where r, g, b and a are floats.
 * @return {Object} An object of the form {grade, title}.
 * |grade| is a string containing the contrast grade.
 * |title| is a string containing the title of the colorwidget.
 */
ColorWidget.calculateGradeAndTitle = function (contrastRatio, backgroundColor) {
  let grade = "";
  let title = "";

  if (contrastRatio < 3.0) {
    grade = L10N.getStr("inspector.colorwidget.contrastRatio.failGrade");
    title = L10N.getStr("inspector.colorwidget.contrastRatio.failInfo");
  } else if (contrastRatio < 4.5) {
    grade = "AA*";
    title = L10N.getStr("inspector.colorwidget.contrastRatio.AABigInfo");
  } else if (contrastRatio < 7.0) {
    grade = "AAA*";
    title = L10N.getStr("inspector.colorwidget.contrastRatio.AAABigInfo");
  } else if (contrastRatio < 22.0) {
    grade = "AAA";
    title = L10N.getStr("inspector.colorwidget.contrastRatio.AAAInfo");
  }
  title += "\n";
  title += L10N.getStr("inspector.colorwidget.contrastRatio.info") + " ";
  title += backgroundColor;

  return { grade, title };
};

/**
 * Converts the contrastRatio to a string of length 4 by rounding
 * contrastRatio and padding the required number of 0s before or
 * after.
 * @param {Number} contrastRatio The contrast ratio to be formatted.
 * @return {String} The formatted ratio.
 */
ColorWidget.ratioToString = function (contrastRatio) {
  let formattedRatio = (contrastRatio < 10) ? "0" : "";
  formattedRatio += contrastRatio.toFixed(2);
  return formattedRatio;
};

ColorWidget.hsvToRgb = function (h, s, v, a) {
  let r, g, b;

  let i = Math.floor(h * 6);
  let f = h * 6 - i;
  let p = v * (1 - s);
  let q = v * (1 - f * s);
  let t = v * (1 - (1 - f) * s);

  switch (i % 6) {
    case 0: r = v; g = t; b = p; break;
    case 1: r = q; g = v; b = p; break;
    case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break;
    case 4: r = t; g = p; b = v; break;
    case 5: r = v; g = p; b = q; break;
  }

  return [r * 255, g * 255, b * 255, a];
};

ColorWidget.rgbToHsv = function (r, g, b, a) {
  r = r / 255;
  g = g / 255;
  b = b / 255;

  let max = Math.max(r, g, b), min = Math.min(r, g, b);
  let h, s, v = max;

  let d = max - min;
  s = max == 0 ? 0 : d / max;

  if (max == min) {
    // achromatic
    h = 0;
  } else {
    switch (max) {
      case r: h = (g - b) / d + (g < b ? 6 : 0); break;
      case g: h = (b - r) / d + 2; break;
      case b: h = (r - g) / d + 4; break;
    }
    h /= 6;
  }
  return [h, s, v, a];
};

ColorWidget.hslToCssString = function (h, s, l, a) {
  return `hsla(${h}, ${s}%, ${l}%, ${a})`;
};

ColorWidget.draggable = function (element, onmove, onstart, onstop) {
  onmove = onmove || function () {};
  onstart = onstart || function () {};
  onstop = onstop || function () {};

  let doc = element.ownerDocument;
  let dragging = false;
  let offset = {};
  let maxHeight = 0;
  let maxWidth = 0;

  function prevent(e) {
    e.stopPropagation();
    e.preventDefault();
  }

  function move(e) {
    if (dragging) {
      if (e.buttons === 0) {
        // The button is no longer pressed but we did not get a mouseup event.
        stop();
        return;
      }
      let pageX = e.pageX;
      let pageY = e.pageY;

      let dragX = Math.max(0, Math.min(pageX - offset.left, maxWidth));
      let dragY = Math.max(0, Math.min(pageY - offset.top, maxHeight));

      onmove.apply(element, [dragX, dragY]);
    }
  }

  function start(e) {
    let rightclick = e.which === 3;

    if (!rightclick && !dragging) {
      if (onstart.apply(element, arguments) !== false) {
        dragging = true;
        maxHeight = element.offsetHeight;
        maxWidth = element.offsetWidth;

        offset = element.getBoundingClientRect();

        move(e);

        doc.addEventListener("selectstart", prevent);
        doc.addEventListener("dragstart", prevent);
        doc.addEventListener("mousemove", move);
        doc.addEventListener("mouseup", stop);

        prevent(e);
      }
    }
  }

  function stop() {
    if (dragging) {
      doc.removeEventListener("selectstart", prevent);
      doc.removeEventListener("dragstart", prevent);
      doc.removeEventListener("mousemove", move);
      doc.removeEventListener("mouseup", stop);
      onstop.apply(element, arguments);
    }
    dragging = false;
  }

  element.addEventListener("mousedown", start);
};

ColorWidget.prototype = {
  set rgb(color) {
    this.hsv = ColorWidget.rgbToHsv(color[0], color[1], color[2], color[3]);

    let { h, s, l } = new colorUtils.CssColor(this.rgbCssString)._getHSLATuple();
    this.hsl = [h, s, l, color[3]];
  },

  get rgb() {
    let rgb = ColorWidget.hsvToRgb(this.hsv[0], this.hsv[1], this.hsv[2],
      this.hsv[3]);
    return [Math.round(rgb[0]), Math.round(rgb[1]), Math.round(rgb[2]),
            Math.round(rgb[3] * 100) / 100];
  },

  get rgbNoSatVal() {
    let rgb = ColorWidget.hsvToRgb(this.hsv[0], 1, 1);
    return [Math.round(rgb[0]), Math.round(rgb[1]), Math.round(rgb[2]), rgb[3]];
  },

  get rgbCssString() {
    let rgb = this.rgb;
    return "rgba(" + rgb[0] + ", " + rgb[1] + ", " + rgb[2] + ", " +
      rgb[3] + ")";
  },

  initializeColorWidget: function () {
    this.parentEl.innerHTML = "";
    this.element = this.parentEl.ownerDocument.createElementNS(XHTML_NS, "div");

    this.element.className = "colorwidget-container";
    this.element.innerHTML = `
      <div class="colorwidget-top">
        <div class="colorwidget-fill"></div>
        <div class="colorwidget-top-inner">
          <div class="colorwidget-color colorwidget-box">
            <div class="colorwidget-sat">
              <div class="colorwidget-val">
                <div class="colorwidget-dragger"></div>
              </div>
            </div>
          </div>
          <div class="colorwidget-hue colorwidget-box">
            <div class="colorwidget-slider colorwidget-slider-control"></div>
          </div>
        </div>
      </div>
      <div class="colorwidget-alpha colorwidget-checker colorwidget-box">
        <div class="colorwidget-alpha-inner">
          <div class="colorwidget-alpha-handle colorwidget-slider-control"></div>
        </div>
      </div>
      <div class="colorwidget-value">
        <select class="colorwidget-select">
          <option value="hex">Hex</option>
          <option value="rgba">RGBA</option>
          <option value="hsla">HSLA</option>
        </select>
        <div class="colorwidget-hex">
          <input class="colorwidget-hex-input"/>
        </div>
        <div class="colorwidget-rgba colorwidget-hidden">
          <input class="colorwidget-rgba-r" data-id="r" />
          <input class="colorwidget-rgba-g" data-id="g" />
          <input class="colorwidget-rgba-b" data-id="b" />
          <input class="colorwidget-rgba-a" data-id="a" />
        </div>
        <div class="colorwidget-hsla colorwidget-hidden">
          <input class="colorwidget-hsla-h" data-id="h" />
          <input class="colorwidget-hsla-s" data-id="s" />
          <input class="colorwidget-hsla-l" data-id="l" />
          <input class="colorwidget-hsla-a" data-id="a" />
        </div>
      </div>
    <div class="colorwidget-contrast">
      <div class="colorwidget-contrast-info"></div>
      <div class="colorwidget-contrast-inner">
        <span class="colorwidget-colorswatch"></span>
        <span class="colorwidget-contrast-ratio"></span>
        <span class="colorwidget-contrast-grade"></span>
        <button class="colorwidget-contrast-help devtools-button"></button>
      </div>
    </div>
    `;

    this.element.addEventListener("click", this.onElementClick);

    this.parentEl.appendChild(this.element);

    this.closestBackgroundColor = "rgba(255, 255, 255, 1)";

    this.contrast = this.element.querySelector(".colorwidget-contrast");
    this.contrastInfo = this.element.querySelector(".colorwidget-contrast-info");
    this.contrastInfo.textContent = L10N.getStr(
      "inspector.colorwidget.contrastRatio.header"
    );

    this.contrastInner = this.element.querySelector(".colorwidget-contrast-inner");
    this.contrastSwatch = this.contrastInner.querySelector(".colorwidget-colorswatch");

    this.contrastSwatch.textContent = SAMPLE_TEXT;

    this.contrastRatio = this.contrastInner.querySelector(".colorwidget-contrast-ratio");
    this.contrastGrade = this.contrastInner.querySelector(".colorwidget-contrast-grade");
    this.contrastHelp = this.contrastInner.querySelector(".colorwidget-contrast-help");

    this.slider = this.element.querySelector(".colorwidget-hue");
    this.slideHelper = this.element.querySelector(".colorwidget-slider");
    ColorWidget.draggable(this.slider, this.onSliderMove);

    this.dragger = this.element.querySelector(".colorwidget-color");
    this.dragHelper = this.element.querySelector(".colorwidget-dragger");
    ColorWidget.draggable(this.dragger, this.onDraggerMove);

    this.alphaSlider = this.element.querySelector(".colorwidget-alpha");
    this.alphaSliderInner = this.element.querySelector(".colorwidget-alpha-inner");
    this.alphaSliderHelper = this.element.querySelector(".colorwidget-alpha-handle");
    ColorWidget.draggable(this.alphaSliderInner, this.onAlphaSliderMove);

    this.colorSelect = this.element.querySelector(".colorwidget-select");
    this.colorSelect.addEventListener("change", this.onSelectValueChange);

    this.hexValue = this.element.querySelector(".colorwidget-hex");
    this.hexValueInput = this.element.querySelector(".colorwidget-hex-input");
    this.hexValueInput.addEventListener("input", this.onHexInputChange);

    this.rgbaValue = this.element.querySelector(".colorwidget-rgba");
    this.rgbaValueInputs = {
      r: this.element.querySelector(".colorwidget-rgba-r"),
      g: this.element.querySelector(".colorwidget-rgba-g"),
      b: this.element.querySelector(".colorwidget-rgba-b"),
      a: this.element.querySelector(".colorwidget-rgba-a"),
    };
    this.rgbaValue.addEventListener("input", this.onRgbaInputChange);

    this.hslaValue = this.element.querySelector(".colorwidget-hsla");
    this.hslaValueInputs = {
      h: this.element.querySelector(".colorwidget-hsla-h"),
      s: this.element.querySelector(".colorwidget-hsla-s"),
      l: this.element.querySelector(".colorwidget-hsla-l"),
      a: this.element.querySelector(".colorwidget-hsla-a"),
    };
    this.hslaValue.addEventListener("input", this.onHslaInputChange);
  },

  show: Task.async(function* () {
    this.initializeColorWidget();
    this.element.classList.add("colorwidget-show");

    this.slideHeight = this.slider.offsetHeight;
    this.dragWidth = this.dragger.offsetWidth;
    this.dragHeight = this.dragger.offsetHeight;
    this.dragHelperHeight = this.dragHelper.offsetHeight;
    this.slideHelperHeight = this.slideHelper.offsetHeight;
    this.alphaSliderWidth = this.alphaSliderInner.offsetWidth;
    this.alphaSliderHelperWidth = this.alphaSliderHelper.offsetWidth;

    if (this.inspector && this.inspector.selection.nodeFront && this.contrastEnabled) {
      let node = this.inspector.selection.nodeFront;
      this.closestBackgroundColor = yield node.getClosestBackgroundColor();
    }
    this.updateContrast();

    this.updateUI();
  }),

  onElementClick: function (e) {
    e.stopPropagation();
  },

  onSliderMove: function (dragX, dragY) {
    this.hsv[0] = (dragY / this.slideHeight);
    this.hsl[0] = (dragY / this.slideHeight) * 360;
    this.updateUI();
    this.onChange();
  },

  onDraggerMove: function (dragX, dragY) {
    this.hsv[1] = dragX / this.dragWidth;
    this.hsv[2] = (this.dragHeight - dragY) / this.dragHeight;

    this.hsl[2] = ((2 - this.hsv[1]) * this.hsv[2] / 2);
    if (this.hsl[2] && this.hsl[2] < 1) {
      this.hsl[1] = this.hsv[1] * this.hsv[2] /
          (this.hsl[2] < 0.5 ? this.hsl[2] * 2 : 2 - this.hsl[2] * 2);
      this.hsl[1] = this.hsl[1] * 100;
    }
    this.hsl[2] = this.hsl[2] * 100;

    this.updateUI();
    this.onChange();
  },

  onAlphaSliderMove: function (dragX, dragY) {
    this.hsv[3] = dragX / this.alphaSliderWidth;
    this.hsl[3] = dragX / this.alphaSliderWidth;
    this.updateUI();
    this.onChange();
  },

  onSelectValueChange: function (event) {
    const selection = event.target.value;
    this.colorSelect.classList.remove("colorwidget-select-spacing");
    this.hexValue.classList.add("colorwidget-hidden");
    this.rgbaValue.classList.add("colorwidget-hidden");
    this.hslaValue.classList.add("colorwidget-hidden");

    switch (selection) {
      case "hex":
        this.hexValue.classList.remove("colorwidget-hidden");
        break;
      case "rgba":
        this.colorSelect.classList.add("colorwidget-select-spacing");
        this.rgbaValue.classList.remove("colorwidget-hidden");
        break;
      case "hsla":
        this.colorSelect.classList.add("colorwidget-select-spacing");
        this.hslaValue.classList.remove("colorwidget-hidden");
        break;
    }
  },

  onHexInputChange: function (event) {
    const hex = event.target.value;
    const color = new colorUtils.CssColor(hex, true);
    if (!color.rgba) {
      return;
    }

    const { r, g, b, a } = color.getRGBATuple();
    this.rgb = [r, g, b, a];
    this.updateUI();
    this.onChange();
  },

  onRgbaInputChange: function (event) {
    const field = event.target.dataset.id;
    const value = event.target.value.toString();
    if (!value || isNaN(value) || value.endsWith(".")) {
      return;
    }

    let rgb = this.rgb;

    switch (field) {
      case "r":
        rgb[0] = value;
        break;
      case "g":
        rgb[1] = value;
        break;
      case "b":
        rgb[2] = value;
        break;
      case "a":
        rgb[3] = Math.min(value, 1);
        break;
    }

    this.rgb = rgb;

    this.updateUI();
    this.onChange();
  },

  onHslaInputChange: function (event) {
    const field = event.target.dataset.id;
    let value = event.target.value.toString();
    if ((field === "s" || field === "l") && !value.endsWith("%")) {
      return;
    }

    if (value.endsWith("%")) {
      value = value.substring(0, value.length - 1);
    }

    if (!value || isNaN(value) || value.endsWith(".")) {
      return;
    }

    const hsl = this.hsl;

    switch (field) {
      case "h":
        hsl[0] = value;
        break;
      case "s":
        hsl[1] = value;
        break;
      case "l":
        hsl[2] = value;
        break;
      case "a":
        hsl[3] = Math.min(value, 1);
        break;
    }

    const cssString = ColorWidget.hslToCssString(hsl[0], hsl[1], hsl[2], hsl[3]);
    const { r, g, b, a } = new colorUtils.CssColor(cssString).getRGBATuple();

    this.rgb = [r, g, b, a];

    this.hsl = hsl;

    this.updateUI();
    this.onChange();
  },

  onChange: function () {
    this.updateContrast();
    this.emit("changed", this.rgb, this.rgbCssString);
  },

  updateContrast: function () {
    if (!this.contrastEnabled) {
      this.contrast.style.display = "none";
      return;
    }

    this.contrast.style.display = "initial";

    if (!colorUtils.isValidCSSColor(this.closestBackgroundColor)) {
      this.contrastRatio.textContent = L10N.getStr(
        "inspector.colorwidget.contrastRatio.invalidColor"
      );

      this.contrastGrade.textContent = "";
      this.contrastHelp.removeAttribute("title");
      return;
    }
    if (!this.rgbaColor) {
      this.rgbaColor = new colorUtils.CssColor(this.closestBackgroundColor);
    }
    this.rgbaColor.newColor(this.closestBackgroundColor);
    let rgba = this.rgbaColor._getRGBATuple();
    let backgroundColor = [rgba.r, rgba.g, rgba.b, rgba.a];

    let textColor = this.rgb;

    let ratio = colorUtils.calculateContrastRatio(backgroundColor, textColor);

    let contrastDetails = ColorWidget.calculateGradeAndTitle(ratio,
                        this.rgbaColor.toString());

    this.contrastRatio.textContent = ColorWidget.ratioToString(ratio);
    this.contrastGrade.textContent = contrastDetails.grade;

    this.contrastHelp.setAttribute("title", contrastDetails.title);

    this.contrastSwatch.style.backgroundColor = this.rgbaColor.toString();
    this.contrastSwatch.style.color = this.rgbCssString;
  },

  updateHelperLocations: function () {
    // If the UI hasn't been shown yet then none of the dimensions will be
    // correct
    if (!this.element.classList.contains("colorwidget-show")) {
      return;
    }

    let h = this.hsv[0];
    let s = this.hsv[1];
    let v = this.hsv[2];

    // Placing the color dragger
    let dragX = s * this.dragWidth;
    let dragY = this.dragHeight - (v * this.dragHeight);
    let helperDim = this.dragHelperHeight / 2;

    dragX = Math.max(
      -helperDim,
      Math.min(this.dragWidth - helperDim, dragX - helperDim)
    );
    dragY = Math.max(
      -helperDim,
      Math.min(this.dragHeight - helperDim, dragY - helperDim)
    );

    this.dragHelper.style.top = dragY + "px";
    this.dragHelper.style.left = dragX + "px";

    // Placing the hue slider
    let slideY = (h * this.slideHeight) - this.slideHelperHeight / 2;
    this.slideHelper.style.top = slideY + "px";

    // Placing the alpha slider
    let alphaSliderX = (this.hsv[3] * this.alphaSliderWidth) -
      (this.alphaSliderHelperWidth / 2);
    this.alphaSliderHelper.style.left = alphaSliderX + "px";

    const color = new colorUtils.CssColor(this.rgbCssString);

    // Updating the hex field
    this.hexValueInput.value = color.hex;

    // Updating the RGBA fields
    const rgb = this.rgb;
    this.rgbaValueInputs.r.value = rgb[0];
    this.rgbaValueInputs.g.value = rgb[1];
    this.rgbaValueInputs.b.value = rgb[2];
    this.rgbaValueInputs.a.value = parseFloat(rgb[3].toFixed(1));

    // Updating the HSLA fields
    this.hslaValueInputs.h.value = this.hsl[0];
    this.hslaValueInputs.s.value = this.hsl[1] + "%";
    this.hslaValueInputs.l.value = this.hsl[2] + "%";
    this.hslaValueInputs.a.value = parseFloat(this.hsl[3].toFixed(1));
  },

  updateUI: function () {
    this.updateHelperLocations();

    let rgb = this.rgb;
    let rgbNoSatVal = this.rgbNoSatVal;

    let flatColor = "rgb(" + rgbNoSatVal[0] + ", " + rgbNoSatVal[1] + ", " +
      rgbNoSatVal[2] + ")";

    this.dragger.style.backgroundColor = flatColor;

    let rgbNoAlpha = "rgb(" + rgb[0] + "," + rgb[1] + "," + rgb[2] + ")";
    let rgbAlpha0 = "rgba(" + rgb[0] + "," + rgb[1] + "," + rgb[2] + ", 0)";
    let alphaGradient = "linear-gradient(to right, " + rgbAlpha0 + ", " +
      rgbNoAlpha + ")";
    this.alphaSliderInner.style.background = alphaGradient;
  },

  destroy: function () {
    this.element.removeEventListener("click", this.onElementClick);

    this.parentEl.removeChild(this.element);

    this.slider = null;
    this.dragger = null;
    this.alphaSlider = this.alphaSliderInner = this.alphaSliderHelper = null;
    this.parentEl = null;
    this.element = null;
  }
};
