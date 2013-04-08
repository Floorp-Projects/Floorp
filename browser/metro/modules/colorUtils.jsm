// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';
Components.utils.import("resource://gre/modules/Services.jsm");
const ColorAnalyzer = Components.classes["@mozilla.org/places/colorAnalyzer;1"]
                .getService(Components.interfaces.mozIColorAnalyzer);

this.EXPORTED_SYMBOLS = ["ColorUtils"];

let ColorUtils = {
  /** Takes an icon and a success callback (who is expected to handle foreground color
   *  & background color as is desired) The first argument to the callback is
   * the foreground/contrast color, the second is the background/primary/dominant one
   */
  getForegroundAndBackgroundIconColors: function getForegroundAndBackgroundIconColors(aIconURI, aSuccessCallback) {
    if (!aIconURI) {
      return;
    }

    let that = this;
    let wrappedIcon = aIconURI;
    ColorAnalyzer.findRepresentativeColor(wrappedIcon, function (success, color) {
      let foregroundColor = that.bestTextColorForContrast(color);
      let backgroundColor = that.convertDecimalToRgbColor(color);
      // let the caller do whatever it is they need through the callback
      aSuccessCallback(foregroundColor, backgroundColor);
    }, this);
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

  /**
   * converts a decimal(base10) number into rgb string
   */
  convertDecimalToRgbColor: function convertDecimalToRgbColor(aColor) {
    let r = (aColor & 0xff0000) >> 16;
    let g = (aColor & 0x00ff00) >> 8;
    let b = (aColor & 0x0000ff);
    return "rgb("+r+","+g+","+b+")";
  }
};
