/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["ScrollbarSampler"];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Promise",
                                  "resource://gre/modules/Promise.jsm");

var gSystemScrollbarWidth = null;

this.ScrollbarSampler = {
  getSystemScrollbarWidth: function() {
    let deferred = Promise.defer();

    if (gSystemScrollbarWidth !== null) {
      deferred.resolve(gSystemScrollbarWidth);
      return deferred.promise;
    }

    this._sampleSystemScrollbarWidth().then(function(systemScrollbarWidth) {
      gSystemScrollbarWidth = systemScrollbarWidth;
      deferred.resolve(gSystemScrollbarWidth);
    });
    return deferred.promise;
  },

  resetSystemScrollbarWidth: function() {
    gSystemScrollbarWidth = null;
  },

  _sampleSystemScrollbarWidth: function() {
    let deferred = Promise.defer();
    let hwin = Services.appShell.hiddenDOMWindow;
    let hdoc = hwin.document.documentElement;
    let iframe = hwin.document.createElementNS("http://www.w3.org/1999/xhtml",
                                               "html:iframe");
    iframe.setAttribute("srcdoc", '<body style="overflow-y: scroll"></body>');
    hdoc.appendChild(iframe);

    let cwindow = iframe.contentWindow;
    let utils = cwindow.QueryInterface(Ci.nsIInterfaceRequestor)
                       .getInterface(Ci.nsIDOMWindowUtils);

    cwindow.addEventListener("load", function onLoad(aEvent) {
      cwindow.removeEventListener("load", onLoad);
      let sbWidth = {};
      try {
        utils.getScrollbarSize(true, sbWidth, {});
      } catch(e) {
        Cu.reportError("Could not sample scrollbar size: " + e + " -- " +
                       e.stack);
        sbWidth.value = 0;
      }
      // Minimum width of 10 so that we have enough padding:
      sbWidth.value = Math.max(sbWidth.value, 10);
      deferred.resolve(sbWidth.value);
      iframe.remove();
    });

    return deferred.promise;
  }
};
Object.freeze(this.ScrollbarSampler);
