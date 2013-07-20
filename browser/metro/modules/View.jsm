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

function View() {
}

View.prototype = {

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
      if (aItem.refresh) {
        aItem.refresh();
      }
    };
    let failureAction = function() {};
    ColorUtils.getForegroundAndBackgroundIconColors(xpFaviconURI, successAction, failureAction);
  }

};
