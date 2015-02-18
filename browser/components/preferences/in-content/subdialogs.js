/* - This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this file,
   - You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let gSubDialog = {
  _closingCallback: null,
  _frame: null,
  _overlay: null,
  _box: null,
  _injectedStyleSheets: ["chrome://mozapps/content/preferences/preferences.css",
                         "chrome://browser/skin/preferences/preferences.css",
                         "chrome://global/skin/in-content/common.css",
                         "chrome://browser/skin/preferences/in-content/preferences.css",
                         "chrome://browser/skin/preferences/in-content/dialog.css"],

  init: function() {
    this._frame = document.getElementById("dialogFrame");
    this._overlay = document.getElementById("dialogOverlay");
    this._box = document.getElementById("dialogBox");

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
    let features = (!!aFeatures ? aFeatures + "," : "") + "resizable,dialog=no,centerscreen";
    let dialog = window.openDialog(aURL, "dialogFrame", features, aParams);
    if (aClosingCallback) {
      this._closingCallback = aClosingCallback.bind(dialog);
    }
    features = features.replace(/,/g, "&");
    let featureParams = new URLSearchParams(features.toLowerCase());
    this._box.setAttribute("resizable", featureParams.has("resizable") &&
                                        featureParams.get("resizable") != "no" &&
                                        featureParams.get("resizable") != "0");
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
    // Clear the sizing attributes
    this._box.removeAttribute("width");
    this._box.removeAttribute("height");
    this._box.style.removeProperty("min-height");
    this._box.style.removeProperty("min-width");

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

    // Provide the ability for the dialog to know that it is being loaded "in-content".
    this._frame.contentDocument.documentElement.setAttribute("subdialog", "true");

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

    let groupBoxTitle = document.getAnonymousElementByAttribute(this._box, "class", "groupbox-title");
    let groupBoxTitleHeight = groupBoxTitle.clientHeight +
                              parseFloat(getComputedStyle(groupBoxTitle).borderBottomWidth);

    let groupBoxBody = document.getAnonymousElementByAttribute(this._box, "class", "groupbox-body");
    let boxVerticalPadding = 2 * parseFloat(getComputedStyle(groupBoxBody).paddingTop);
    let boxHorizontalPadding = 2 * parseFloat(getComputedStyle(groupBoxBody).paddingLeft);
    let frameWidth = docEl.style.width || docEl.scrollWidth + "px";
    let frameHeight = docEl.style.height || docEl.scrollHeight + "px";
    let boxVerticalBorder = 2 * parseFloat(getComputedStyle(this._box).borderTopWidth);
    let boxHorizontalBorder = 2 * parseFloat(getComputedStyle(this._box).borderLeftWidth);

    let frameRect = this._frame.getBoundingClientRect();
    let boxRect = this._box.getBoundingClientRect();
    let frameSizeDifference = (frameRect.top - boxRect.top) + (boxRect.bottom - frameRect.bottom);

    // Now check if the frame height we calculated is possible at this window size,
    // accounting for titlebar, padding/border and some spacing.
    let maxHeight = window.innerHeight - frameSizeDifference - 30;
    if (frameHeight > maxHeight) {
      // If not, we should probably let the dialog scroll:
      frameHeight = maxHeight;
      let containers = this._frame.contentDocument.querySelectorAll('.largeDialogContainer');
      for (let container of containers) {
        container.classList.add("doScroll");
      }
    }

    this._frame.style.width = frameWidth;
    this._frame.style.height = frameHeight;
    this._box.style.minHeight = "calc(" +
                                (boxVerticalBorder + groupBoxTitleHeight + boxVerticalPadding) +
                                "px + " + frameHeight + ")";
    this._box.style.minWidth = "calc(" +
                               (boxHorizontalBorder + boxHorizontalPadding) +
                               "px + " + frameWidth + ")";

    this._overlay.style.visibility = "visible";
    this._frame.focus();
    this._overlay.style.opacity = ""; // XXX: focus hack continued from _onContentLoaded
  },
};
