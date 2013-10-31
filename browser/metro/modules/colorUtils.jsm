// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';
Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/commonjs/sdk/core/promise.js");

const ColorAnalyzer = Components.classes["@mozilla.org/places/colorAnalyzer;1"]
                .getService(Components.interfaces.mozIColorAnalyzer);

this.EXPORTED_SYMBOLS = ["ColorUtils"];

let ColorUtils = {
  _initialized: false,
  init: function() {
    if (this._initialized)
        return;
    Services.obs.addObserver(this, "idle-daily", false);
    Services.obs.addObserver(this, "quit-application", false);
    this._initialized = true;
  },
  uninit: function() {
    Services.obs.removeObserver(this, "idle-daily");
    Services.obs.removeObserver(this, "quit-application");
  },

  // default to keeping icon colorInfo for max 1 day
  iconColorCacheMaxAge: 86400000,

  // in-memory store for favicon color data
  _uriColorsMap: (function() {
    let cache = new Map();
    // remove stale entries
    cache.purge = function(maxAgeMs = 0) {
      let cuttoff = Date.now() - (maxAgeMs || ColorUtils.iconColorCacheMaxAge);
      for (let [key, value] of this) {
        if (value.timestamp && value.timestamp >= cuttoff) {
          continue;
        }
        this.delete(key);
      }
    }
    return cache;
  })(),
  get iconColorCache() {
    return ColorUtils._uriColorsMap;
  },

  observe: function (aSubject, aTopic, aData) {
    switch (aTopic) {
      case "idle-daily":
        this.iconColorCache.purge();
        break;
      case "quit-application":
        this.uninit();
        break;
    }
  },

  /** Takes an icon and returns either an object with foreground and background color properties
   *  or a promise for the same.
   *  The foreground is the contrast color, the background is the primary/dominant one
   */
  getForegroundAndBackgroundIconColors: function getForegroundAndBackgroundIconColors(aIconURI) {
    let colorKey = aIconURI.spec;
    let colorInfo = this._uriColorsMap.get(colorKey);
    if (colorInfo) {
      return colorInfo;
    }

    let deferred = Promise.defer();
    let wrappedIcon = aIconURI;
    this._uriColorsMap.set(colorKey, deferred.promise);

    ColorAnalyzer.findRepresentativeColor(wrappedIcon, (success, color) => {
      if (!success) {
        this._uriColorsMap.delete(colorKey);
        deferred.reject();
      } else {
        colorInfo = {
          foreground: this.bestTextColorForContrast(color),
          background: this.convertDecimalToRgbColor(color),
          timestamp: Date.now()
        };
        deferred.resolve(colorInfo);
      }
    });
    return deferred.promise;
  },

  /** returns the best color for text readability on top of aColor
   * return color is in rgb(r,g,b) format, suitable to csss
   * The color bightness algorithm is currently: http://www.w3.org/TR/AERT#color-contrast
   */
  bestTextColorForContrast: function bestTextColorForContrast(aColor) {
    let r = (aColor & 0xff0000) >> 16;
    let g = (aColor & 0x00ff00) >> 8;
    let b = (aColor & 0x0000ff);

    let w3cContrastValue = ((r*299)+(g*587)+(b*114))/1000;
    w3cContrastValue = Math.round(w3cContrastValue);
    let textColor = "rgb(255,255,255)";

    if (w3cContrastValue > 125) {
      // bright/light, use black text
      textColor = "rgb(0,0,0)";
    }
    return textColor;
  },

  toCSSRgbColor: function toCSSRgbColor(r, g, b, a) {
    var values = [Math.round(r), Math.round(g), Math.round(b)];
    if (undefined !== a && a < 1) {
      values.push(a);
      return 'rgba('+values.join(',')+')';
    }
    return 'rgb('+values.join(',')+')';
  },

  /**
   * converts a decimal(base10) number into CSS rgb color value string
   */
  convertDecimalToRgbColor: function convertDecimalToRgbColor(aColor) {
    let [r,g,b,a] = this.unpackDecimalColorWord(aColor);
    return this.toCSSRgbColor(r,g,b,a);
  },

  /**
   * unpack a decimal(base10) word for r,g,b,a values
   */
  unpackDecimalColorWord: function unpackDecimalColorWord(aColor) {
    let a = (aColor & 0xff000000) >> 24;
    let r = (aColor & 0x00ff0000) >> 16;
    let g = (aColor & 0x0000ff00) >> 8;
    let b = (aColor & 0x000000ff);
    // NB: falsy alpha treated as undefined, fully opaque
    return a ? [r,g,b,a/255] : [r,g,b];
  },

  /**
   * create a decimal(base10) word for r,g,b values
   */
  createDecimalColorWord: function createDecimalColorWord(r, g, b, a) {
    let rgb = 0;
    rgb |= b;
    rgb |= (g << 8);
    rgb |= (r << 16);
    // pack alpha value if one is given
    if (undefined !== a && a < 1)
      rgb |= (Math.round(a*255) << 24);
    return rgb;
  },

  /**
   * Add 2 rgb(a) colors to get a flat color
   */
  addRgbColors: function addRgbColors(color1, color2) {
    let [r1, g1, b1] = this.unpackDecimalColorWord(color1);
    let [r2, g2, b2, alpha] = this.unpackDecimalColorWord(color2);

    let color = {};
    // early return if 2nd color is opaque
    if (!alpha || alpha >= 1)
      return color2;

    return this.createDecimalColorWord(
      Math.min(255, alpha * r2 + (1 - alpha) * r1),
      Math.min(255, alpha * g2 + (1 - alpha) * g1),
      Math.min(255, alpha * b2 + (1 - alpha) * b1)
    );
  }
};
