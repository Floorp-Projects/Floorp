/* - This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this file,
   - You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from ../../../base/content/utilityOverlay.js */
/* import-globals-from preferences.js */

"use strict";

/**
 * SubDialog constructor creates a new subdialog from a template and appends
 * it to the parentElement.
 * @param {DOMNode} template: The template is copied to create a new dialog.
 * @param {DOMNode} parentElement: New dialog is appended onto parentElement.
 * @param {String}  id: A unique identifier for the dialog.
 */
function SubDialog({template, parentElement, id}) {
  this._id = id;

  this._overlay = template.cloneNode(true);
  this._frame = this._overlay.querySelector(".dialogFrame");
  this._box = this._overlay.querySelector(".dialogBox");
  this._closeButton = this._overlay.querySelector(".dialogClose");
  this._titleElement = this._overlay.querySelector(".dialogTitle");

  this._overlay.id = `dialogOverlay-${id}`;
  this._frame.setAttribute("name", `dialogFrame-${id}`);
  this._frameCreated = new Promise(resolve => {
    this._frame.addEventListener("load", resolve, {once: true});
  });

  parentElement.appendChild(this._overlay);
  this._overlay.hidden = false;
}

SubDialog.prototype = {
  _closingCallback: null,
  _closingEvent: null,
  _isClosing: false,
  _frame: null,
  _frameCreated: null,
  _overlay: null,
  _box: null,
  _openedURL: null,
  _injectedStyleSheets: [
    "chrome://browser/skin/preferences/preferences.css",
    "chrome://global/skin/in-content/common.css",
    "chrome://browser/skin/preferences/in-content-new/preferences.css",
    "chrome://browser/skin/preferences/in-content-new/dialog.css",
  ],
  _resizeObserver: null,
  _template: null,
  _id: null,
  _titleElement: null,
  _closeButton: null,

  updateTitle(aEvent) {
    if (aEvent.target != this._frame.contentDocument)
      return;
    this._titleElement.textContent = this._frame.contentDocument.title;
  },

  injectXMLStylesheet(aStylesheetURL) {
    let contentStylesheet = this._frame.contentDocument.createProcessingInstruction(
      "xml-stylesheet",
      'href="' + aStylesheetURL + '" type="text/css"'
    );
    this._frame.contentDocument.insertBefore(contentStylesheet,
                                             this._frame.contentDocument.documentElement);
  },

  async open(aURL, aFeatures = null, aParams = null, aClosingCallback = null) {
    // Wait until frame is ready to prevent browser crash in tests
    await this._frameCreated;
    // If we're open on some (other) URL or we're closing, open when closing has finished.
    if (this._openedURL || this._isClosing) {
      if (!this._isClosing) {
        this.close();
      }
      let args = Array.from(arguments);
      this._closingPromise.then(() => {
        this.open.apply(this, args);
      });
      return;
    }
    this._addDialogEventListeners();

    let features = (aFeatures ? aFeatures + "," : "") + "resizable,dialog=no,centerscreen";
    let dialog = window.openDialog(aURL, `dialogFrame-${this._id}`, features, aParams);
    if (aClosingCallback) {
      this._closingCallback = aClosingCallback.bind(dialog);
    }

    this._closingEvent = null;
    this._isClosing = false;
    this._openedURL = aURL;

    features = features.replace(/,/g, "&");
    let featureParams = new URLSearchParams(features.toLowerCase());
    this._box.setAttribute("resizable", featureParams.has("resizable") &&
                                        featureParams.get("resizable") != "no" &&
                                        featureParams.get("resizable") != "0");
  },

  close(aEvent = null) {
    if (this._isClosing) {
      return;
    }
    this._isClosing = true;
    this._closingPromise = new Promise(resolve => {
      this._resolveClosePromise = resolve;
    });

    if (this._closingCallback) {
      try {
        this._closingCallback.call(null, aEvent);
      } catch (ex) {
        Cu.reportError(ex);
      }
      this._closingCallback = null;
    }

    this._removeDialogEventListeners();

    this._overlay.style.visibility = "";
    // Clear the sizing inline styles.
    this._frame.removeAttribute("style");
    // Clear the sizing attributes
    this._box.removeAttribute("width");
    this._box.removeAttribute("height");
    this._box.style.removeProperty("min-height");
    this._box.style.removeProperty("min-width");

    this._overlay.dispatchEvent(new CustomEvent("dialogclose", {
      bubbles: true,
      detail: { dialog: this },
    }));

    setTimeout(() => {
      // Unload the dialog after the event listeners run so that the load of about:blank isn't
      // cancelled by the ESC <key>.
      let onBlankLoad = e => {
        if (this._frame.contentWindow.location.href == "about:blank") {
          this._frame.removeEventListener("load", onBlankLoad);
          // We're now officially done closing, so update the state to reflect that.
          this._openedURL = null;
          this._isClosing = false;
          this._resolveClosePromise();
        }
      };
      this._frame.addEventListener("load", onBlankLoad);
      this._frame.loadURI("about:blank");
    }, 0);
  },

  handleEvent(aEvent) {
    switch (aEvent.type) {
      case "click":
        // Close the dialog if the user clicked the overlay background, just
        // like when the user presses the ESC key (case "command" below).
        if (aEvent.target === this._overlay) {
          this._frame.contentWindow.close();
        }
        break;
      case "command":
        this._frame.contentWindow.close();
        break;
      case "dialogclosing":
        this._onDialogClosing(aEvent);
        break;
      case "DOMTitleChanged":
        this.updateTitle(aEvent);
        break;
      case "DOMFrameContentLoaded":
        this._onContentLoaded(aEvent);
        break;
      case "load":
        this._onLoad(aEvent);
        break;
      case "unload":
        this._onUnload(aEvent);
        break;
      case "keydown":
        this._onKeyDown(aEvent);
        break;
      case "focus":
        this._onParentWinFocus(aEvent);
        break;
    }
  },

  /* Private methods */

  _onUnload(aEvent) {
    if (aEvent.target.location.href == this._openedURL) {
      this._frame.contentWindow.close();
    }
  },

  _onContentLoaded(aEvent) {
    if (aEvent.target != this._frame || aEvent.target.contentWindow.location == "about:blank") {
      return;
    }

    for (let styleSheetURL of this._injectedStyleSheets) {
      this.injectXMLStylesheet(styleSheetURL);
    }

    // Provide the ability for the dialog to know that it is being loaded "in-content".
    this._frame.contentDocument.documentElement.setAttribute("subdialog", "true");

    this._frame.contentWindow.addEventListener("dialogclosing", this);

    let oldResizeBy = this._frame.contentWindow.resizeBy;
    this._frame.contentWindow.resizeBy = (resizeByWidth, resizeByHeight) => {
      // Only handle resizeByHeight currently.
      let frameHeight = this._frame.clientHeight;
      let boxMinHeight = parseFloat(getComputedStyle(this._box).minHeight, 10);

      this._frame.style.height = (frameHeight + resizeByHeight) + "px";
      this._box.style.minHeight = (boxMinHeight + resizeByHeight) + "px";

      oldResizeBy.call(this._frame.contentWindow, resizeByWidth, resizeByHeight);
    };

    // Make window.close calls work like dialog closing.
    let oldClose = this._frame.contentWindow.close;
    this._frame.contentWindow.close = () => {
      var closingEvent = this._closingEvent;
      if (!closingEvent) {
        closingEvent = new CustomEvent("dialogclosing", {
          bubbles: true,
          detail: { button: null },
        });

        this._frame.contentWindow.dispatchEvent(closingEvent);
      }

      this.close(closingEvent);
      oldClose.call(this._frame.contentWindow);
    };

    // XXX: Hack to make focus during the dialog's load functions work. Make the element visible
    // sooner in DOMContentLoaded but mostly invisible instead of changing visibility just before
    // the dialog's load event.
    this._overlay.style.visibility = "visible";
    this._overlay.style.opacity = "0.01";
  },

  _onLoad(aEvent) {
    if (aEvent.target.contentWindow.location == "about:blank") {
      return;
    }

    // Do this on load to wait for the CSS to load and apply before calculating the size.
    let docEl = this._frame.contentDocument.documentElement;

    let groupBoxTitle = document.getAnonymousElementByAttribute(this._box, "class", "groupbox-title");
    let groupBoxTitleHeight = groupBoxTitle.clientHeight +
                              parseFloat(getComputedStyle(groupBoxTitle).borderBottomWidth);

    let groupBoxBody = document.getAnonymousElementByAttribute(this._box, "class", "groupbox-body");
    // These are deduced from styles which we don't change, so it's safe to get them now:
    let boxVerticalPadding = 2 * parseFloat(getComputedStyle(groupBoxBody).paddingTop);
    let boxHorizontalPadding = 2 * parseFloat(getComputedStyle(groupBoxBody).paddingLeft);
    let boxHorizontalBorder = 2 * parseFloat(getComputedStyle(this._box).borderLeftWidth);
    let boxVerticalBorder = 2 * parseFloat(getComputedStyle(this._box).borderTopWidth);

    // The difference between the frame and box shouldn't change, either:
    let boxRect = this._box.getBoundingClientRect();
    let frameRect = this._frame.getBoundingClientRect();
    let frameSizeDifference = (frameRect.top - boxRect.top) + (boxRect.bottom - frameRect.bottom);

    // Then determine and set a bunch of width stuff:
    let frameMinWidth = docEl.style.width || docEl.scrollWidth + "px";
    let frameWidth = docEl.getAttribute("width") ? docEl.getAttribute("width") + "px" :
                     frameMinWidth;
    this._frame.style.width = frameWidth;
    this._box.style.minWidth = "calc(" +
                               (boxHorizontalBorder + boxHorizontalPadding) +
                               "px + " + frameMinWidth + ")";

    // Now do the same but for the height. We need to do this afterwards because otherwise
    // XUL assumes we'll optimize for height and gives us "wrong" values which then are no
    // longer correct after we set the width:
    let frameMinHeight = docEl.style.height || docEl.scrollHeight + "px";
    let frameHeight = docEl.getAttribute("height") ? docEl.getAttribute("height") + "px" :
                                                     frameMinHeight;

    // Now check if the frame height we calculated is possible at this window size,
    // accounting for titlebar, padding/border and some spacing.
    let maxHeight = window.innerHeight - frameSizeDifference - 30;
    // Do this with a frame height in pixels...
    let comparisonFrameHeight;
    if (frameHeight.endsWith("em")) {
      let fontSize = parseFloat(getComputedStyle(this._frame).fontSize);
      comparisonFrameHeight = parseFloat(frameHeight, 10) * fontSize;
    } else if (frameHeight.endsWith("px")) {
      comparisonFrameHeight = parseFloat(frameHeight, 10);
    } else {
      Cu.reportError("This dialog (" + this._frame.contentWindow.location.href + ") " +
                     "set a height in non-px-non-em units ('" + frameHeight + "'), " +
                     "which is likely to lead to bad sizing in in-content preferences. " +
                     "Please consider changing this.");
      comparisonFrameHeight = parseFloat(frameHeight);
    }

    if (comparisonFrameHeight > maxHeight) {
      // If the height is bigger than that of the window, we should let the contents scroll:
      frameHeight = maxHeight + "px";
      frameMinHeight = maxHeight + "px";
      let containers = this._frame.contentDocument.querySelectorAll(".largeDialogContainer");
      for (let container of containers) {
        container.classList.add("doScroll");
      }
    }

    this._frame.style.height = frameHeight;
    this._box.style.minHeight = "calc(" +
                                (boxVerticalBorder + groupBoxTitleHeight + boxVerticalPadding) +
                                "px + " + frameMinHeight + ")";

    this._overlay.dispatchEvent(new CustomEvent("dialogopen", {
      bubbles: true,
      detail: { dialog: this },
    }));
    this._overlay.style.visibility = "visible";
    this._overlay.style.opacity = ""; // XXX: focus hack continued from _onContentLoaded

    if (this._box.getAttribute("resizable") == "true") {
      this._resizeObserver = new MutationObserver(this._onResize);
      this._resizeObserver.observe(this._box, {attributes: true});
    }

    this._trapFocus();

    // Search within main document and highlight matched keyword.
    gSearchResultsPane.searchWithinNode(this._titleElement, gSearchResultsPane.query);

    // Search within sub-dialog document and highlight matched keyword.
    let subDialogsChildren = this._frame.contentDocument
      .querySelectorAll(":scope > *:not([data-hidden-from-search])");

    for (let i = 0; i < subDialogsChildren.length; i++) {
      gSearchResultsPane.searchWithinNode(subDialogsChildren[i], gSearchResultsPane.query);
    }

    // Creating tooltips for all the instances found
    for (let node of gSearchResultsPane.listSearchTooltips) {
      if (!node.getAttribute("data-has-tooltip")) {
        gSearchResultsPane.createSearchTooltip(node, gSearchResultsPane.query);
      }
    }
  },

  _onResize(mutations) {
    let frame = this._frame;
    // The width and height styles are needed for the initial
    // layout of the frame, but afterward they need to be removed
    // or their presence will restrict the contents of the <browser>
    // from resizing to a smaller size.
    frame.style.removeProperty("width");
    frame.style.removeProperty("height");

    let docEl = frame.contentDocument.documentElement;
    let persistedAttributes = docEl.getAttribute("persist");
    if (!persistedAttributes ||
        (!persistedAttributes.includes("width") &&
         !persistedAttributes.includes("height"))) {
      return;
    }

    for (let mutation of mutations) {
      if (mutation.attributeName == "width") {
        docEl.setAttribute("width", docEl.scrollWidth);
      } else if (mutation.attributeName == "height") {
        docEl.setAttribute("height", docEl.scrollHeight);
      }
    }
  },

  _onDialogClosing(aEvent) {
    this._frame.contentWindow.removeEventListener("dialogclosing", this);
    this._closingEvent = aEvent;
  },

  _onKeyDown(aEvent) {
    if (aEvent.currentTarget == window && aEvent.keyCode == aEvent.DOM_VK_ESCAPE &&
        !aEvent.defaultPrevented) {
      this.close(aEvent);
      return;
    }
    if (aEvent.keyCode != aEvent.DOM_VK_TAB ||
        aEvent.ctrlKey || aEvent.altKey || aEvent.metaKey) {
      return;
    }

    let fm = Services.focus;

    let isLastFocusableElement = el => {
      // XXXgijs unfortunately there is no way to get the last focusable element without asking
      // the focus manager to move focus to it.
      let rv = el == fm.moveFocus(this._frame.contentWindow, null, fm.MOVEFOCUS_LAST, 0);
      fm.setFocus(el, 0);
      return rv;
    };

    let forward = !aEvent.shiftKey;
    // check if focus is leaving the frame (incl. the close button):
    if ((aEvent.target == this._closeButton && !forward) ||
        (isLastFocusableElement(aEvent.originalTarget) && forward)) {
      aEvent.preventDefault();
      aEvent.stopImmediatePropagation();
      let parentWin = this._getBrowser().ownerGlobal;
      if (forward) {
        fm.moveFocus(parentWin, null, fm.MOVEFOCUS_FIRST, fm.FLAG_BYKEY);
      } else {
        // Somehow, moving back 'past' the opening doc is not trivial. Cheat by doing it in 2 steps:
        fm.moveFocus(window, null, fm.MOVEFOCUS_ROOT, fm.FLAG_BYKEY);
        fm.moveFocus(parentWin, null, fm.MOVEFOCUS_BACKWARD, fm.FLAG_BYKEY);
      }
    }
  },

  _onParentWinFocus(aEvent) {
    // Explicitly check for the focus target of |window| to avoid triggering this when the window
    // is refocused
    if (aEvent.target != this._closeButton && aEvent.target != window) {
      this._closeButton.focus();
    }
  },

  _addDialogEventListeners() {
    // Make the close button work.
    this._closeButton.addEventListener("command", this);

    // DOMTitleChanged isn't fired on the frame, only on the chromeEventHandler
    let chromeBrowser = this._getBrowser();
    chromeBrowser.addEventListener("DOMTitleChanged", this, true);

    // Similarly DOMFrameContentLoaded only fires on the top window
    window.addEventListener("DOMFrameContentLoaded", this, true);

    // Wait for the stylesheets injected during DOMContentLoaded to load before showing the dialog
    // otherwise there is a flicker of the stylesheet applying.
    this._frame.addEventListener("load", this);

    chromeBrowser.addEventListener("unload", this, true);

    // Ensure we get <esc> keypresses even if nothing in the subdialog is focusable
    // (happens on OS X when only text inputs and lists are focusable, and
    //  the subdialog only has checkboxes/radiobuttons/buttons)
    window.addEventListener("keydown", this, true);

    this._overlay.addEventListener("click", this, true);
  },

  _removeDialogEventListeners() {
    let chromeBrowser = this._getBrowser();
    chromeBrowser.removeEventListener("DOMTitleChanged", this, true);
    chromeBrowser.removeEventListener("unload", this, true);

    this._closeButton.removeEventListener("command", this);

    window.removeEventListener("DOMFrameContentLoaded", this, true);
    this._frame.removeEventListener("load", this);
    this._frame.contentWindow.removeEventListener("dialogclosing", this);
    window.removeEventListener("keydown", this, true);

    this._overlay.removeEventListener("click", this, true);

    if (this._resizeObserver) {
      this._resizeObserver.disconnect();
      this._resizeObserver = null;
    }
    this._untrapFocus();
  },

  _trapFocus() {
    let fm = Services.focus;
    fm.moveFocus(this._frame.contentWindow, null, fm.MOVEFOCUS_FIRST, 0);
    this._frame.contentDocument.addEventListener("keydown", this, true);
    this._closeButton.addEventListener("keydown", this);

    window.addEventListener("focus", this, true);
  },

  _untrapFocus() {
    this._frame.contentDocument.removeEventListener("keydown", this, true);
    this._closeButton.removeEventListener("keydown", this);
    window.removeEventListener("focus", this);
  },

  _getBrowser() {
    return window.QueryInterface(Ci.nsIInterfaceRequestor)
                 .getInterface(Ci.nsIWebNavigation)
                 .QueryInterface(Ci.nsIDocShell)
                 .chromeEventHandler;
  },
};

var gSubDialog = {
  /**
   * New dialogs are stacked on top of the existing ones, and they are pushed
   * to the end of the _dialogs array.
   * @type {Array}
   */
  _dialogs: [],
  _dialogStack: null,
  _dialogTemplate: null,
  _nextDialogID: 0,
  _preloadDialog: null,
  get _topDialog() {
    return this._dialogs.length > 0 ? this._dialogs[this._dialogs.length - 1] : undefined;
  },

  init() {
    this._dialogStack = document.getElementById("dialogStack");
    this._dialogTemplate = document.getElementById("dialogTemplate");
    this._preloadDialog = new SubDialog({template: this._dialogTemplate,
                                         parentElement: this._dialogStack,
                                         id: this._nextDialogID++});
  },

  open(aURL, aFeatures = null, aParams = null, aClosingCallback = null) {
    // If we're already open/opening on this URL, do nothing.
    if (this._topDialog && this._topDialog._openedURL == aURL) {
      return;
    }

    this._preloadDialog.open(aURL, aFeatures, aParams, aClosingCallback);
    this._dialogs.push(this._preloadDialog);
    this._preloadDialog = new SubDialog({template: this._dialogTemplate,
                                         parentElement: this._dialogStack,
                                         id: this._nextDialogID++});

    if (this._dialogs.length == 1) {
      this._dialogStack.hidden = false;
      this._ensureStackEventListeners();
    }
  },

  close() {
    this._topDialog.close();
  },

  handleEvent(aEvent) {
    switch (aEvent.type) {
      case "dialogopen": {
        this._onDialogOpen();
        break;
      }
      case "dialogclose": {
        this._onDialogClose(aEvent.detail.dialog);
        break;
      }
    }
  },

  _onDialogOpen() {
    let lowerDialog = this._dialogs.length > 1 ? this._dialogs[this._dialogs.length - 2] : undefined;
    if (lowerDialog) {
      lowerDialog._overlay.removeAttribute("topmost");
      lowerDialog._removeDialogEventListeners();
    }
  },

  _onDialogClose(dialog) {
    let fm = Services.focus;
    if (this._topDialog == dialog) {
      // XXX: When a top-most dialog is closed, we reuse the closed dialog and
      //      remove the preloadDialog. This is a temporary solution before we
      //      rewrite all the test cases in Bug 1359023.
      this._preloadDialog._overlay.remove();
      this._preloadDialog = this._dialogs.pop();
    } else {
      dialog._overlay.remove();
      this._dialogs.splice(this._dialogs.indexOf(dialog), 1);
    }

    if (this._topDialog) {
      fm.moveFocus(this._topDialog._frame.contentWindow, null, fm.MOVEFOCUS_FIRST, fm.FLAG_BYKEY);
      this._topDialog._overlay.setAttribute("topmost", true);
      this._topDialog._addDialogEventListeners();
    } else {
      fm.moveFocus(window, null, fm.MOVEFOCUS_ROOT, fm.FLAG_BYKEY);
      this._dialogStack.hidden = true;
      this._removeStackEventListeners();
    }
  },

  _ensureStackEventListeners() {
    this._dialogStack.addEventListener("dialogopen", this);
    this._dialogStack.addEventListener("dialogclose", this);
  },

  _removeStackEventListeners() {
    this._dialogStack.removeEventListener("dialogopen", this);
    this._dialogStack.removeEventListener("dialogclose", this);
  },
};
