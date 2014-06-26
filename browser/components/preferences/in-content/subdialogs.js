/* - This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this file,
   - You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let gSubDialog = {
  _closingCallback: null,
  _frame: null,
  _overlay: null,
  _injectedStyleSheets: ["chrome://mozapps/content/preferences/preferences.css",
                         "chrome://browser/skin/preferences/preferences.css",
                         "chrome://browser/skin/preferences/in-content/preferences.css"],

  init: function() {
    this._frame = document.getElementById("dialogFrame");
    this._overlay = document.getElementById("dialogOverlay");

    // Make the close button work.
    let dialogClose = document.getElementById("dialogClose");
    dialogClose.addEventListener("command", this.close.bind(this));

    // DOMTitleChanged isn't fired on the frame, only on the chromeEventHandler
    let chromeBrowser = window.QueryInterface(Ci.nsIInterfaceRequestor)
                              .getInterface(Ci.nsIWebNavigation)
                              .QueryInterface(Ci.nsIDocShell)
                              .chromeEventHandler;
    chromeBrowser.addEventListener("DOMTitleChanged", this.updateTitle, true);

    // Similarly DOMFrameContentLoaded only fires on the top window
    window.addEventListener("DOMFrameContentLoaded", this._onContentLoaded.bind(this), true);

    // Wait for the stylesheets injected during DOMContentLoaded to load before showing the dialog
    // otherwise there is a flicker of the stylesheet applying.
    this._frame.addEventListener("load", this._onLoad.bind(this));
  },

  uninit: function() {
    let chromeBrowser = window.QueryInterface(Ci.nsIInterfaceRequestor)
                              .getInterface(Ci.nsIWebNavigation)
                              .QueryInterface(Ci.nsIDocShell)
                              .chromeEventHandler;
    chromeBrowser.removeEventListener("DOMTitleChanged", gSubDialog.updateTitle, true);
  },

  updateTitle: function(aEvent) {
    if (aEvent.target != gSubDialog._frame.contentDocument)
      return;
    document.getElementById("dialogTitle").textContent = gSubDialog._frame.contentDocument.title;
  },

  injectXMLStylesheet: function(aStylesheetURL) {
    let contentStylesheet = this._frame.contentDocument.createProcessingInstruction(
      'xml-stylesheet',
      'href="' + aStylesheetURL + '" type="text/css"'
    );
    this._frame.contentDocument.insertBefore(contentStylesheet,
                                             this._frame.contentDocument.documentElement);
  },

  open: function(aURL, aFeatures = null, aParams = null, aClosingCallback = null) {
    let features = aFeatures || "modal,centerscreen,resizable=no";
    let dialog = window.openDialog(aURL, "dialogFrame", features, aParams);
    if (aClosingCallback) {
      this._closingCallback = aClosingCallback.bind(dialog);
    }
    return dialog;
  },

  close: function(aEvent = null) {
    if (this._closingCallback) {
      try {
        this._closingCallback.call(null, aEvent);
      } catch (ex) {
        Cu.reportError(ex);
      }
      this._closingCallback = null;
    }

    this._overlay.style.visibility = "";
    // Clear the sizing inline styles.
    this._frame.removeAttribute("style");

    setTimeout(() => {
      // Unload the dialog after the event listeners run so that the load of about:blank isn't
      // cancelled by the ESC <key>.
      this._frame.loadURI("about:blank");
    }, 0);
  },

  /* Private methods */

  _onContentLoaded: function(aEvent) {
    if (aEvent.target != this._frame || aEvent.target.contentWindow.location == "about:blank")
      return;

    for (let styleSheetURL of this._injectedStyleSheets) {
      this.injectXMLStylesheet(styleSheetURL);
    }

    // Make window.close calls work like dialog closing.
    let oldClose = this._frame.contentWindow.close;
    this._frame.contentWindow.close = function() {
      var closingEvent = new CustomEvent("dialogclosing", {
        bubbles: true,
        detail: { button: null },
      });
      gSubDialog._frame.contentWindow.dispatchEvent(closingEvent);

      oldClose.call(gSubDialog._frame.contentWindow);
    };

    // XXX: Hack to make focus during the dialog's load functions work. Make the element visible
    // sooner in DOMContentLoaded but mostly invisible instead of changing visibility just before
    // the dialog's load event.
    this._overlay.style.visibility = "visible";
    this._overlay.style.opacity = "0.01";

    this._frame.contentWindow.addEventListener("dialogclosing", function closingDialog(aEvent) {
      gSubDialog._frame.contentWindow.removeEventListener("dialogclosing", closingDialog);
      gSubDialog.close(aEvent);
    });
  },

  _onLoad: function(aEvent) {
    if (aEvent.target.contentWindow.location == "about:blank")
      return;

    // Do this on load to wait for the CSS to load and apply before calculating the size.
    let docEl = this._frame.contentDocument.documentElement;
    this._frame.style.width = docEl.style.width || docEl.scrollWidth + "px";
    this._frame.style.height = docEl.style.height || docEl.scrollHeight + "px";

    this._overlay.style.visibility = "visible";
    this._frame.focus();
    this._overlay.style.opacity = ""; // XXX: focus hack continued from _onContentLoaded
  },
};
