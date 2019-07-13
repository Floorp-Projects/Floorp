/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EventEmitter = require("devtools/shared/event-emitter");
const { MultiLocalizationHelper } = require("devtools/shared/l10n");
const L10N = new MultiLocalizationHelper(
  "devtools/shared/locales/en-US/accessibility.properties",
  "devtools/client/locales/en-US/accessibility.properties",
  "devtools/client/locales/en-US/inspector.properties"
);
const ARROW_KEYS = ["ArrowUp", "ArrowRight", "ArrowDown", "ArrowLeft"];
const [ArrowUp, ArrowRight, ArrowDown, ArrowLeft] = ARROW_KEYS;
const XHTML_NS = "http://www.w3.org/1999/xhtml";
const COLOR_HEX_WHITE = "#ffffff";
const SLIDER = {
  hue: {
    MIN: "0",
    MAX: "128",
    STEP: "1",
  },
  alpha: {
    MIN: "0",
    MAX: "1",
    STEP: "0.01",
  },
};

loader.lazyRequireGetter(this, "colorUtils", "devtools/shared/css/color", true);
loader.lazyRequireGetter(
  this,
  "cssColors",
  "devtools/shared/css/color-db",
  true
);
loader.lazyRequireGetter(
  this,
  "labColors",
  "devtools/shared/css/color-db",
  true
);
loader.lazyRequireGetter(
  this,
  "getContrastRatioScore",
  "devtools/shared/accessibility",
  true
);
loader.lazyRequireGetter(
  this,
  "getTextProperties",
  "devtools/shared/accessibility",
  true
);

/**
 * Spectrum creates a color picker widget in any container you give it.
 *
 * Simple usage example:
 *
 * const {Spectrum} = require("devtools/client/shared/widgets/Spectrum");
 * let s = new Spectrum(containerElement, [255, 126, 255, 1]);
 * s.on("changed", (rgba, color) => {
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
 * visible will allow spectrum to correctly initialize its various parts.
 *
 * Fires the following events:
 * - changed : When the user changes the current color
 */
function Spectrum(parentEl, rgb) {
  EventEmitter.decorate(this);

  this.document = parentEl.ownerDocument;
  this.element = parentEl.ownerDocument.createElementNS(XHTML_NS, "div");
  this.parentEl = parentEl;

  this.element.className = "spectrum-container";
  // eslint-disable-next-line no-unsanitized/property
  this.element.innerHTML = `
    <section class="spectrum-color-picker">
      <div
        class="spectrum-color spectrum-box"
        tabindex="0"
        role="slider"
        title="${L10N.getStr("colorPickerTooltip.spectrumDraggerTitle")}"
        aria-describedby="spectrum-dragger"
      >
        <div class="spectrum-sat">
          <div class="spectrum-val">
            <div class="spectrum-dragger" id="spectrum-dragger"></div>
          </div>
        </div>
      </div>
    </section>
    <section class="spectrum-controls">
      <div class="spectrum-color-preview"></div>
      <div class="spectrum-slider-container">
        <div class="spectrum-hue spectrum-box"></div>
        <div class="spectrum-alpha spectrum-checker spectrum-box"></div>
      </div>
    </section>
    <section class="spectrum-color-contrast accessibility-color-contrast">
      <span class="contrast-ratio-label" role="presentation">
        ${L10N.getStr("accessibility.contrast.ratio.label")}
      </span>
      <span class="accessibility-contrast-value"></span>
      <span
        class="accessibility-color-contrast-large-text"
        title="${L10N.getStr("accessibility.contrast.large.title")}"
      >
        ${L10N.getStr("accessibility.contrast.large.text")}
      </span>
    </section>
  `;

  this.onElementClick = this.onElementClick.bind(this);
  this.element.addEventListener("click", this.onElementClick);

  this.parentEl.appendChild(this.element);

  // Color spectrum dragger.
  this.dragger = this.element.querySelector(".spectrum-color");
  this.dragHelper = this.element.querySelector(".spectrum-dragger");
  Spectrum.draggable(
    this.dragger,
    this.dragHelper,
    this.onDraggerMove.bind(this)
  );

  // Here we define the components for the "controls" section of the color picker.
  this.controls = this.element.querySelector(".spectrum-controls");
  this.colorPreview = this.element.querySelector(".spectrum-color-preview");

  // Create the eyedropper.
  const eyedropper = this.document.createElementNS(XHTML_NS, "button");
  eyedropper.id = "eyedropper-button";
  eyedropper.className = "devtools-button";
  eyedropper.style.pointerEvents = "auto";
  eyedropper.setAttribute(
    "aria-label",
    L10N.getStr("colorPickerTooltip.eyedropperTitle")
  );
  this.controls.insertBefore(eyedropper, this.colorPreview);

  // Hue slider and alpha slider
  this.hueSlider = this.createSlider("hue", this.onHueSliderMove.bind(this));
  this.hueSlider.setAttribute("aria-describedby", this.dragHelper.id);
  this.alphaSlider = this.createSlider(
    "alpha",
    this.onAlphaSliderMove.bind(this)
  );

  // Color contrast
  this.spectrumContrast = this.element.querySelector(
    ".spectrum-color-contrast"
  );
  this.contrastValue = this.element.querySelector(
    ".accessibility-contrast-value"
  );

  // Create the learn more info button
  const learnMore = this.document.createElementNS(XHTML_NS, "button");
  learnMore.id = "learn-more-button";
  learnMore.className = "learn-more";
  learnMore.title = L10N.getStr("accessibility.learnMore");
  this.spectrumContrast.appendChild(learnMore);

  if (rgb) {
    this.rgb = rgb;
    this.updateUI();
  }
}

module.exports.Spectrum = Spectrum;

Spectrum.hsvToRgb = function(h, s, v, a) {
  let r, g, b;

  const i = Math.floor(h * 6);
  const f = h * 6 - i;
  const p = v * (1 - s);
  const q = v * (1 - f * s);
  const t = v * (1 - (1 - f) * s);

  switch (i % 6) {
    case 0:
      r = v;
      g = t;
      b = p;
      break;
    case 1:
      r = q;
      g = v;
      b = p;
      break;
    case 2:
      r = p;
      g = v;
      b = t;
      break;
    case 3:
      r = p;
      g = q;
      b = v;
      break;
    case 4:
      r = t;
      g = p;
      b = v;
      break;
    case 5:
      r = v;
      g = p;
      b = q;
      break;
  }

  return [r * 255, g * 255, b * 255, a];
};

Spectrum.rgbToHsv = function(r, g, b, a) {
  r = r / 255;
  g = g / 255;
  b = b / 255;

  const max = Math.max(r, g, b);
  const min = Math.min(r, g, b);

  const v = max;
  const d = max - min;
  const s = max == 0 ? 0 : d / max;

  let h;
  if (max == min) {
    // achromatic
    h = 0;
  } else {
    switch (max) {
      case r:
        h = (g - b) / d + (g < b ? 6 : 0);
        break;
      case g:
        h = (b - r) / d + 2;
        break;
      case b:
        h = (r - g) / d + 4;
        break;
    }
    h /= 6;
  }
  return [h, s, v, a];
};

Spectrum.draggable = function(element, dragHelper, onmove) {
  onmove = onmove || function() {};

  const doc = element.ownerDocument;
  let dragging = false;
  let offset = {};
  let maxHeight = 0;
  let maxWidth = 0;

  function setDraggerDimensionsAndOffset() {
    maxHeight = element.offsetHeight;
    maxWidth = element.offsetWidth;
    offset = element.getBoundingClientRect();
  }

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
      const pageX = e.pageX;
      const pageY = e.pageY;

      const dragX = Math.max(0, Math.min(pageX - offset.left, maxWidth));
      const dragY = Math.max(0, Math.min(pageY - offset.top, maxHeight));

      onmove.apply(element, [dragX, dragY]);
    }
  }

  function start(e) {
    const rightClick = e.which === 3;

    if (!rightClick && !dragging) {
      dragging = true;
      setDraggerDimensionsAndOffset();

      move(e);

      doc.addEventListener("selectstart", prevent);
      doc.addEventListener("dragstart", prevent);
      doc.addEventListener("mousemove", move);
      doc.addEventListener("mouseup", stop);

      prevent(e);
    }
  }

  function stop() {
    if (dragging) {
      doc.removeEventListener("selectstart", prevent);
      doc.removeEventListener("dragstart", prevent);
      doc.removeEventListener("mousemove", move);
      doc.removeEventListener("mouseup", stop);
    }
    dragging = false;
  }

  function onKeydown(e) {
    const { key } = e;

    if (!ARROW_KEYS.includes(key)) {
      return;
    }

    setDraggerDimensionsAndOffset();
    const { offsetHeight, offsetTop, offsetLeft } = dragHelper;
    let dragX = offsetLeft + offsetHeight / 2;
    let dragY = offsetTop + offsetHeight / 2;

    if (key === ArrowLeft && dragX > 0) {
      dragX -= 1;
    } else if (key === ArrowRight && dragX < maxWidth) {
      dragX += 1;
    } else if (key === ArrowUp && dragY > 0) {
      dragY -= 1;
    } else if (key === ArrowDown && dragY < maxHeight) {
      dragY += 1;
    }

    onmove.apply(element, [dragX, dragY]);
  }

  element.addEventListener("mousedown", start);
  element.addEventListener("keydown", onKeydown);
};

/**
 * Calculates the contrast ratio for a DOM node's computed style against
 * a given background.
 *
 * @param  {Object} computedStyle
 *         The computed style for which we want to calculate the contrast ratio.
 * @param  {Array} background
 *         The rgba value array for background color (i.e. [255, 255, 255, 1]).
 * @return {Object}
 *         An object that may contain one or more of the following fields: error,
 *         isLargeText, value, score for contrast.
 */
function getContrastRatioAgainstSolidBg(
  computedStyle,
  background = cssColors.white
) {
  const props = getTextProperties(computedStyle);
  if (!props) {
    return {
      error: true,
    };
  }

  const { color, isLargeText } = props;
  const value = colorUtils.calculateContrastRatio(background, color);
  return {
    value,
    color,
    isLargeText,
    score: getContrastRatioScore(value, isLargeText),
  };
}

Spectrum.prototype = {
  set textProps(style) {
    this._textProps = style
      ? {
          fontSize: style["font-size"].value,
          fontWeight: style["font-weight"].value,
        }
      : null;
  },

  set rgb(color) {
    this.hsv = Spectrum.rgbToHsv(color[0], color[1], color[2], color[3]);
  },

  get textProps() {
    return this._textProps;
  },

  get rgb() {
    const rgb = Spectrum.hsvToRgb(
      this.hsv[0],
      this.hsv[1],
      this.hsv[2],
      this.hsv[3]
    );
    return [
      Math.round(rgb[0]),
      Math.round(rgb[1]),
      Math.round(rgb[2]),
      Math.round(rgb[3] * 100) / 100,
    ];
  },

  /**
   * Map current rgb to the closest color available in the database by
   * calculating the delta-E between each available color and the current rgb
   *
   * @return {String}
   *         Color name or closest color name
   */
  get colorName() {
    const labColorEntries = Object.entries(labColors);

    const deltaEs = labColorEntries.map(color =>
      colorUtils.calculateDeltaE(color[1], colorUtils.rgbToLab(this.rgb))
    );

    // Get the color name for the one that has the lowest delta-E
    const minDeltaE = Math.min(...deltaEs);
    const colorName = labColorEntries[deltaEs.indexOf(minDeltaE)][0];
    return minDeltaE === 0
      ? colorName
      : L10N.getFormatStr("colorPickerTooltip.colorNameTitle", colorName);
  },

  get rgbNoSatVal() {
    const rgb = Spectrum.hsvToRgb(this.hsv[0], 1, 1);
    return [Math.round(rgb[0]), Math.round(rgb[1]), Math.round(rgb[2]), rgb[3]];
  },

  get rgbCssString() {
    const rgb = this.rgb;
    return (
      "rgba(" + rgb[0] + ", " + rgb[1] + ", " + rgb[2] + ", " + rgb[3] + ")"
    );
  },

  show: function() {
    this.dragWidth = this.dragger.offsetWidth;
    this.dragHeight = this.dragger.offsetHeight;
    this.dragHelperHeight = this.dragHelper.offsetHeight;

    this.updateUI();
  },

  onElementClick: function(e) {
    e.stopPropagation();
  },

  onHueSliderMove: function() {
    this.hsv[0] = this.hueSlider.value / this.hueSlider.max;
    this.updateUI();
    this.onChange();
  },

  onDraggerMove: function(dragX, dragY) {
    this.hsv[1] = dragX / this.dragWidth;
    this.hsv[2] = (this.dragHeight - dragY) / this.dragHeight;
    this.updateUI();
    this.onChange();
  },

  onAlphaSliderMove: function() {
    this.hsv[3] = this.alphaSlider.value / this.alphaSlider.max;
    this.updateUI();
    this.onChange();
  },

  onChange: function() {
    this.emit("changed", this.rgb, this.rgbCssString);
  },

  /**
   * Creates and initializes a slider element, attaches it to its parent container
   * based on the slider type and returns it
   *
   * @param  {String} sliderType
   *         The type of the slider (i.e. alpha or hue)
   * @param  {Function} onSliderMove
   *         The function to tie the slider to on input
   * @return {DOMNode}
   *         Newly created slider
   */
  createSlider: function(sliderType, onSliderMove) {
    const container = this.element.querySelector(`.spectrum-${sliderType}`);

    const slider = this.document.createElementNS(XHTML_NS, "input");
    slider.className = `spectrum-${sliderType}-input`;
    slider.type = "range";
    slider.min = SLIDER[sliderType].MIN;
    slider.max = SLIDER[sliderType].MAX;
    slider.step = SLIDER[sliderType].STEP;
    slider.title = L10N.getStr(`colorPickerTooltip.${sliderType}SliderTitle`);
    slider.addEventListener("input", onSliderMove);

    container.appendChild(slider);
    return slider;
  },

  updateAlphaSlider: function() {
    // Set alpha slider background
    const rgb = this.rgb;

    const rgbNoAlpha = "rgb(" + rgb[0] + "," + rgb[1] + "," + rgb[2] + ")";
    const rgbAlpha0 = "rgba(" + rgb[0] + "," + rgb[1] + "," + rgb[2] + ", 0)";
    const alphaGradient =
      "linear-gradient(to right, " + rgbAlpha0 + ", " + rgbNoAlpha + ")";
    this.alphaSlider.style.background = alphaGradient;
  },

  updateColorPreview: function() {
    // Overlay the rgba color over a checkered image background.
    this.colorPreview.style.setProperty("--overlay-color", this.rgbCssString);

    // We should be able to distinguish the color preview on high luminance rgba values.
    // Give the color preview a light grey border if the luminance of the current rgba
    // tuple is great.
    const colorLuminance = colorUtils.calculateLuminance(this.rgb);
    this.colorPreview.classList.toggle("high-luminance", colorLuminance > 0.85);

    // Set title on color preview for better UX
    this.colorPreview.title = this.colorName;
  },

  updateDragger: function() {
    // Set dragger background color
    const flatColor =
      "rgb(" +
      this.rgbNoSatVal[0] +
      ", " +
      this.rgbNoSatVal[1] +
      ", " +
      this.rgbNoSatVal[2] +
      ")";
    this.dragger.style.backgroundColor = flatColor;

    // Set dragger aria attributes
    this.dragger.setAttribute("aria-valuetext", this.rgbCssString);
  },

  updateHueSlider: function() {
    // Set hue slider aria attributes
    this.hueSlider.setAttribute("aria-valuetext", this.rgbCssString);
  },

  updateHelperLocations: function() {
    const h = this.hsv[0];
    const s = this.hsv[1];
    const v = this.hsv[2];

    // Placing the color dragger
    let dragX = s * this.dragWidth;
    let dragY = this.dragHeight - v * this.dragHeight;
    const helperDim = this.dragHelperHeight / 2;

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
    this.hueSlider.value = h * this.hueSlider.max;

    // Placing the alpha slider
    this.alphaSlider.value = this.hsv[3] * this.alphaSlider.max;
  },

  /* Calculates the contrast ratio for the currently selected
   * color against white background and displays/hides contrast ratio span
   * components depending on the contrast ratio calculated.
   *
   * Contrast ratio components include:
   *    - contrastLargeTextIndicator: Hidden by default, shown when text has large font
   *                                  size if there is no error in calculation.
   *    - contrastValue:              Set to calculated value and score. Set to error text
   *                                  if there is an error in calculation.
   */
  updateContrast: function() {
    // Remove additional classes on spectrum contrast, leaving behind only base classes
    this.spectrumContrast.classList.toggle("large-text", false);
    this.spectrumContrast.classList.toggle("visible", false);
    // Assign only base class to contrastValue, removing any score class
    this.contrastValue.className = "accessibility-contrast-value";

    if (!this.contrastEnabled) {
      return;
    }

    this.spectrumContrast.classList.toggle("visible", true);

    const contrastRatio = getContrastRatioAgainstSolidBg({
      ...this.textProps,
      color: this.rgbCssString,
    });
    const { value, score, isLargeText, error } = contrastRatio;

    if (error) {
      this.contrastValue.textContent = L10N.getStr(
        "accessibility.contrast.error"
      );
      this.contrastValue.title = L10N.getStr(
        "accessibility.contrast.annotation.transparent.error"
      );
      this.spectrumContrast.classList.remove("large-text");
      return;
    }

    this.contrastValue.classList.toggle(score, true);
    this.contrastValue.textContent = value.toFixed(2);
    this.contrastValue.title = L10N.getFormatStr(
      `accessibility.contrast.annotation.${score}`,
      L10N.getFormatStr(
        "colorPickerTooltip.contrastAgainstBgTitle",
        COLOR_HEX_WHITE
      )
    );
    this.spectrumContrast.classList.toggle("large-text", isLargeText);
  },

  updateUI: function() {
    this.updateHelperLocations();

    this.updateColorPreview();
    this.updateDragger();
    this.updateHueSlider();
    this.updateAlphaSlider();
    this.updateContrast();
  },

  destroy: function() {
    this.element.removeEventListener("click", this.onElementClick);
    this.hueSlider.removeEventListener("input", this.onHueSliderMove);
    this.alphaSlider.removeEventListener("input", this.onAlphaSliderMove);

    this.parentEl.removeChild(this.element);

    this.dragger = this.dragHelper = null;
    this.alphaSlider = null;
    this.hueSlider = null;
    this.colorPreview = null;
    this.element = null;
    this.parentEl = null;
    this.spectrumContrast = null;
    this.contrastValue = null;
  },
};
