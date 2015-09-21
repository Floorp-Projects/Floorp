/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EventEmitter = require("devtools/shared/event-emitter");

/**
 * Spectrum creates a color picker widget in any container you give it.
 *
 * Simple usage example:
 *
 * const {Spectrum} = require("devtools/client/shared/widgets/Spectrum");
 * let s = new Spectrum(containerElement, [255, 126, 255, 1]);
 * s.on("changed", (event, rgba, color) => {
 *   console.log("rgba(" + rgba[0] + ", " + rgba[1] + ", " + rgba[2] + ", " + rgba[3] + ")");
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

  this.element = parentEl.ownerDocument.createElement('div');
  this.parentEl = parentEl;

  this.element.className = "spectrum-container";
  this.element.innerHTML = [
    "<div class='spectrum-top'>",
      "<div class='spectrum-fill'></div>",
      "<div class='spectrum-top-inner'>",
        "<div class='spectrum-color spectrum-box'>",
          "<div class='spectrum-sat'>",
            "<div class='spectrum-val'>",
              "<div class='spectrum-dragger'></div>",
            "</div>",
          "</div>",
        "</div>",
        "<div class='spectrum-hue spectrum-box'>",
          "<div class='spectrum-slider spectrum-slider-control'></div>",
        "</div>",
      "</div>",
    "</div>",
    "<div class='spectrum-alpha spectrum-checker spectrum-box'>",
      "<div class='spectrum-alpha-inner'>",
        "<div class='spectrum-alpha-handle spectrum-slider-control'></div>",
      "</div>",
    "</div>",
  ].join("");

  this.onElementClick = this.onElementClick.bind(this);
  this.element.addEventListener("click", this.onElementClick, false);

  this.parentEl.appendChild(this.element);

  this.slider = this.element.querySelector(".spectrum-hue");
  this.slideHelper = this.element.querySelector(".spectrum-slider");
  Spectrum.draggable(this.slider, this.onSliderMove.bind(this));

  this.dragger = this.element.querySelector(".spectrum-color");
  this.dragHelper = this.element.querySelector(".spectrum-dragger");
  Spectrum.draggable(this.dragger, this.onDraggerMove.bind(this));

  this.alphaSlider = this.element.querySelector(".spectrum-alpha");
  this.alphaSliderInner = this.element.querySelector(".spectrum-alpha-inner");
  this.alphaSliderHelper = this.element.querySelector(".spectrum-alpha-handle");
  Spectrum.draggable(this.alphaSliderInner, this.onAlphaSliderMove.bind(this));

  if (rgb) {
    this.rgb = rgb;
    this.updateUI();
  }
}

module.exports.Spectrum = Spectrum;

Spectrum.hsvToRgb = function(h, s, v, a) {
  let r, g, b;

  let i = Math.floor(h * 6);
  let f = h * 6 - i;
  let p = v * (1 - s);
  let q = v * (1 - f * s);
  let t = v * (1 - (1 - f) * s);

  switch(i % 6) {
    case 0: r = v, g = t, b = p; break;
    case 1: r = q, g = v, b = p; break;
    case 2: r = p, g = v, b = t; break;
    case 3: r = p, g = q, b = v; break;
    case 4: r = t, g = p, b = v; break;
    case 5: r = v, g = p, b = q; break;
  }

  return [r * 255, g * 255, b * 255, a];
};

Spectrum.rgbToHsv = function(r, g, b, a) {
  r = r / 255;
  g = g / 255;
  b = b / 255;

  let max = Math.max(r, g, b), min = Math.min(r, g, b);
  let h, s, v = max;

  let d = max - min;
  s = max == 0 ? 0 : d / max;

  if(max == min) {
    h = 0; // achromatic
  }
  else {
    switch(max) {
      case r: h = (g - b) / d + (g < b ? 6 : 0); break;
      case g: h = (b - r) / d + 2; break;
      case b: h = (r - g) / d + 4; break;
    }
    h /= 6;
  }
  return [h, s, v, a];
};

Spectrum.getOffset = function(el) {
  let curleft = 0, curtop = 0;
  if (el.offsetParent) {
    while (el) {
      curleft += el.offsetLeft;
      curtop += el.offsetTop;
      el = el.offsetParent;
    }
  }
  return {
    left: curleft,
    top: curtop
  };
};

Spectrum.draggable = function(element, onmove, onstart, onstop) {
  onmove = onmove || function() {};
  onstart = onstart || function() {};
  onstop = onstop || function() {};

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
        return stop();
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

        offset = Spectrum.getOffset(element);

        move(e);

        doc.addEventListener("selectstart", prevent, false);
        doc.addEventListener("dragstart", prevent, false);
        doc.addEventListener("mousemove", move, false);
        doc.addEventListener("mouseup", stop, false);

        prevent(e);
      }
    }
  }

  function stop() {
    if (dragging) {
      doc.removeEventListener("selectstart", prevent, false);
      doc.removeEventListener("dragstart", prevent, false);
      doc.removeEventListener("mousemove", move, false);
      doc.removeEventListener("mouseup", stop, false);
      onstop.apply(element, arguments);
    }
    dragging = false;
  }

  element.addEventListener("mousedown", start, false);
};

Spectrum.prototype = {
  set rgb(color) {
    this.hsv = Spectrum.rgbToHsv(color[0], color[1], color[2], color[3]);
  },

  get rgb() {
    let rgb = Spectrum.hsvToRgb(this.hsv[0], this.hsv[1], this.hsv[2], this.hsv[3]);
    return [Math.round(rgb[0]), Math.round(rgb[1]), Math.round(rgb[2]), Math.round(rgb[3]*100)/100];
  },

  get rgbNoSatVal() {
    let rgb = Spectrum.hsvToRgb(this.hsv[0], 1, 1);
    return [Math.round(rgb[0]), Math.round(rgb[1]), Math.round(rgb[2]), rgb[3]];
  },

  get rgbCssString() {
    let rgb = this.rgb;
    return "rgba(" + rgb[0] + ", " + rgb[1] + ", " + rgb[2] + ", " + rgb[3] + ")";
  },

  show: function() {
    this.element.classList.add('spectrum-show');

    this.slideHeight = this.slider.offsetHeight;
    this.dragWidth = this.dragger.offsetWidth;
    this.dragHeight = this.dragger.offsetHeight;
    this.dragHelperHeight = this.dragHelper.offsetHeight;
    this.slideHelperHeight = this.slideHelper.offsetHeight;
    this.alphaSliderWidth = this.alphaSliderInner.offsetWidth;
    this.alphaSliderHelperWidth = this.alphaSliderHelper.offsetWidth;

    this.updateUI();
  },

  onElementClick: function(e) {
    e.stopPropagation();
  },

  onSliderMove: function(dragX, dragY) {
    this.hsv[0] = (dragY / this.slideHeight);
    this.updateUI();
    this.onChange();
  },

  onDraggerMove: function(dragX, dragY) {
    this.hsv[1] = dragX / this.dragWidth;
    this.hsv[2] = (this.dragHeight - dragY) / this.dragHeight;
    this.updateUI();
    this.onChange();
  },

  onAlphaSliderMove: function(dragX, dragY) {
    this.hsv[3] = dragX / this.alphaSliderWidth;
    this.updateUI();
    this.onChange();
  },

  onChange: function() {
    this.emit("changed", this.rgb, this.rgbCssString);
  },

  updateHelperLocations: function() {
    // If the UI hasn't been shown yet then none of the dimensions will be correct
    if (!this.element.classList.contains('spectrum-show'))
      return;

    let h = this.hsv[0];
    let s = this.hsv[1];
    let v = this.hsv[2];

    // Placing the color dragger
    let dragX = s * this.dragWidth;
    let dragY = this.dragHeight - (v * this.dragHeight);
    let helperDim = this.dragHelperHeight/2;

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
    let slideY = (h * this.slideHeight) - this.slideHelperHeight/2;
    this.slideHelper.style.top = slideY + "px";

    // Placing the alpha slider
    let alphaSliderX = (this.hsv[3] * this.alphaSliderWidth) - (this.alphaSliderHelperWidth / 2);
    this.alphaSliderHelper.style.left = alphaSliderX + "px";
  },

  updateUI: function() {
    this.updateHelperLocations();

    let rgb = this.rgb;
    let rgbNoSatVal = this.rgbNoSatVal;

    let flatColor = "rgb(" + rgbNoSatVal[0] + ", " + rgbNoSatVal[1] + ", " + rgbNoSatVal[2] + ")";
    let fullColor = "rgba(" + rgb[0] + ", " + rgb[1] + ", " + rgb[2] + ", " + rgb[3] + ")";

    this.dragger.style.backgroundColor = flatColor;

    var rgbNoAlpha = "rgb(" + rgb[0] + "," + rgb[1] + "," + rgb[2] + ")";
    var rgbAlpha0 = "rgba(" + rgb[0] + "," + rgb[1] + "," + rgb[2] + ", 0)";
    var alphaGradient = "linear-gradient(to right, " + rgbAlpha0 + ", " + rgbNoAlpha + ")";
    this.alphaSliderInner.style.background = alphaGradient;
  },

  destroy: function() {
    this.element.removeEventListener("click", this.onElementClick, false);

    this.parentEl.removeChild(this.element);

    this.slider = null;
    this.dragger = null;
    this.alphaSlider = this.alphaSliderInner = this.alphaSliderHelper = null;
    this.parentEl = null;
    this.element = null;
  }
};
