/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

this.EXPORTED_SYMBOLS = ["View"];

Components.utils.import("resource://gre/modules/PlacesUtils.jsm");
Components.utils.import("resource:///modules/colorUtils.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

// --------------------------------
// module helpers
//

function makeURI(aURL, aOriginCharset, aBaseURI) {
  return Services.io.newURI(aURL, aOriginCharset, aBaseURI);
}

// --------------------------------


// --------------------------------
// View prototype for shared functionality

function View(aSet) {
  this._set = aSet;
  this._set.controller = this;

  this.viewStateObserver = {
    observe: (aSubject, aTopic, aData) => this._adjustDOMforViewState(aData)
  };
  Services.obs.addObserver(this.viewStateObserver, "metro_viewstate_changed", false);

  this._adjustDOMforViewState();
}

View.prototype = {
  destruct: function () {
    Services.obs.removeObserver(this.viewStateObserver, "metro_viewstate_changed");
  },

  _adjustDOMforViewState: function _adjustDOMforViewState(aState) {
    if (this._set) {
      if (undefined == aState)
        aState = this._set.getAttribute("viewstate");

      this._set.setAttribute("suppressonselect", (aState == "snapped"));

      if (aState == "portrait") {
        this._set.setAttribute("vertical", true);
      } else {
        this._set.removeAttribute("vertical");
      }

      this._set.arrangeItems();
    }
  },

  _updateFavicon: function pv__updateFavicon(aItem, aUri) {
    if ("string" == typeof aUri) {
      aUri = makeURI(aUri);
    }
    PlacesUtils.favicons.getFaviconURLForPage(aUri, this._gotIcon.bind(this, aItem));
  },

  _gotIcon: function pv__gotIcon(aItem, aIconUri) {
    if (!aIconUri) {
      aItem.removeAttribute("iconURI");
      if (aItem.refresh) {
        aItem.refresh();
      }
      return;
    }
    if ("string" == typeof aIconUri) {
      aIconUri = makeURI(aIconUri);
    }
    aItem.iconSrc = aIconUri.spec;
    let faviconURL = (PlacesUtils.favicons.getFaviconLinkForIcon(aIconUri)).spec;
    let xpFaviconURI = makeURI(faviconURL.replace("moz-anno:favicon:",""));
    let successAction = function(foreground, background) {
      aItem.style.color = foreground; //color text
      aItem.setAttribute("customColor", background);
      let matteColor =  0xffffff; // white
      let alpha = 0.04; // the tint weight
      let [,r,g,b] = background.match(/rgb\((\d+),(\d+),(\d+)/);
      // get the rgb value that represents this color at given opacity over a white matte
      let tintColor = ColorUtils.addRgbColors(matteColor, ColorUtils.createDecimalColorWord(r,g,b,alpha));
      aItem.setAttribute("tintColor", ColorUtils.convertDecimalToRgbColor(tintColor));
      // when bound, use the setter to propogate the color change through the tile
      if ('color' in aItem) {
        aItem.color = background;
      }
    };
    let failureAction = function() {};
    ColorUtils.getForegroundAndBackgroundIconColors(xpFaviconURI, successAction, failureAction);
  }

};
