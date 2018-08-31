/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["ScrollbarSampler"];

ChromeUtils.import("resource://gre/modules/Services.jsm");

var gSystemScrollbarWidth = null;

var ScrollbarSampler = {
  getSystemScrollbarWidth() {
    if (gSystemScrollbarWidth !== null) {
      return Promise.resolve(gSystemScrollbarWidth);
    }

    return new Promise(resolve => {
      this._sampleSystemScrollbarWidth().then(function(systemScrollbarWidth) {
        gSystemScrollbarWidth = systemScrollbarWidth;
        resolve(gSystemScrollbarWidth);
      });
    });
  },

  resetSystemScrollbarWidth() {
    gSystemScrollbarWidth = null;
  },

  _sampleSystemScrollbarWidth() {
    let hwin = Services.appShell.hiddenDOMWindow;
    let hdoc = hwin.document.documentElement;
    let iframe = hwin.document.createElementNS("http://www.w3.org/1999/xhtml",
                                               "html:iframe");
    iframe.setAttribute("srcdoc", '<body style="overflow-y: scroll"></body>');
    hdoc.appendChild(iframe);

    let cwindow = iframe.contentWindow;
    let utils = cwindow.windowUtils;

    return new Promise(resolve => {
      cwindow.addEventListener("load", function(aEvent) {
        let sbWidth = {};
        try {
          utils.getScrollbarSize(true, sbWidth, {});
        } catch (e) {
          Cu.reportError("Could not sample scrollbar size: " + e + " -- " +
                         e.stack);
          sbWidth.value = 0;
        }
        // Minimum width of 10 so that we have enough padding:
        sbWidth.value = Math.max(sbWidth.value, 10);
        resolve(sbWidth.value);
        iframe.remove();
      }, {once: true});
    });
  },
};
Object.freeze(this.ScrollbarSampler);
