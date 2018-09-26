/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["UrlbarInput"];

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.jsm",
  QueryContext: "resource:///modules/UrlbarController.jsm",
  Services: "resource://gre/modules/Services.jsm",
  UrlbarController: "resource:///modules/UrlbarController.jsm",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.jsm",
  UrlbarView: "resource:///modules/UrlbarView.jsm",
});

/**
 * Represents the urlbar <textbox>.
 * Also forwards important textbox properties and methods.
 */
class UrlbarInput {
  /**
   * @param {object} options
   *   The initial options for UrlbarInput.
   * @param {object} options.textbox
   *   The <textbox> element.
   * @param {object} options.panel
   *   The <panel> element.
   * @param {UrlbarController} [options.controller]
   *   Optional fake controller to override the built-in UrlbarController.
   *   Intended for use in unit tests only.
   */
  constructor(options = {}) {
    this.textbox = options.textbox;
    this.panel = options.panel;
    this.window = this.textbox.ownerGlobal;
    this.controller = options.controller || new UrlbarController();
    this.view = new UrlbarView(this);
    this.valueIsTyped = false;
    this.userInitiatedFocus = false;
    this.isPrivate = PrivateBrowsingUtils.isWindowPrivate(this.window);

    const METHODS = ["addEventListener", "removeEventListener",
      "setAttribute", "hasAttribute", "removeAttribute", "getAttribute",
      "focus", "blur", "select"];
    const READ_ONLY_PROPERTIES = ["focused", "inputField", "editor"];
    const READ_WRITE_PROPERTIES = ["value", "placeholder", "readOnly",
      "selectionStart", "selectionEnd"];

    for (let method of METHODS) {
      this[method] = (...args) => {
        this.textbox[method](...args);
      };
    }

    for (let property of READ_ONLY_PROPERTIES) {
      Object.defineProperty(this, property, {
        enumerable: true,
        get() {
          let getter = "_get_" + property;
          if (getter in this) {
            return this[getter]();
          }
          return this.textbox[property];
        },
      });
    }

    for (let property of READ_WRITE_PROPERTIES) {
      Object.defineProperty(this, property, {
        enumerable: true,
        get() {
          let getter = "_get_" + property;
          if (getter in this) {
            return this[getter]();
          }
          return this.textbox[property];
        },
        set(val) {
          let setter = "_set_" + property;
          if (setter in this) {
            return this[setter](val);
          }
          return this.textbox[property] = val;
        },
      });
    }

    this.addEventListener("input", this);
    this.inputField.addEventListener("select", this);
    this.inputField.addEventListener("overflow", this);
    this.inputField.addEventListener("underflow", this);
    this.inputField.addEventListener("scrollend", this);

    this.inputField.controllers.insertControllerAt(0, new CopyCutController(this));
  }

  /* Shortens the given value, usually by removing http:// and trailing slashes,
   * such that calling nsIURIFixup::createFixupURI with the result will produce
   * the same URI.
   *
   * @param {string} val
   *   The string to be trimmed if it appears to be URI
   */
  trimValue(val) {
    return UrlbarPrefs.get("trimURLs") ? this.window.trimURL(val) : val;
  }

  formatValue() {
  }

  closePopup() {
    this.view.close();
  }

  openResults() {
    this.view.open();
  }

  /**
   * Converts an internal URI (e.g. a wyciwyg URI) into one which we can
   * expose to the user.
   *
   * @param {nsIURI} uri
   *   The URI to be converted
   * @returns {nsIURI}
   *   The converted, exposable URI
   */
  makeURIReadable(uri) {
    // Avoid copying 'about:reader?url=', and always provide the original URI:
    // Reader mode ensures we call createExposableURI itself.
    let readerStrippedURI = this.window.ReaderMode.getOriginalUrlObjectForDisplay(uri.displaySpec);
    if (readerStrippedURI) {
      return readerStrippedURI;
    }

    try {
      return Services.uriFixup.createExposableURI(uri);
    } catch (ex) {}

    return uri;
  }

  /**
   * Passes DOM events for the textbox to the _on<event type> methods.
   * @param {Event} event
   *   DOM event from the <textbox>.
   */
  handleEvent(event) {
    let methodName = "_on" + event.type;
    if (methodName in this) {
      this[methodName](event);
    } else {
      throw "Unrecognized urlbar event: " + event.type;
    }
  }

  // Getters and Setters below.

  _set_value(val) {
    val = this.trimValue(val);

    this.valueIsTyped = false;
    this.inputField.value = val;

    return val;
  }

  // Private methods below.

  _updateTextOverflow() {
    if (!this._inOverflow) {
      this.removeAttribute("textoverflow");
      return;
    }

    this.window.promiseDocumentFlushed(() => {
      // Check overflow again to ensure it didn't change in the meantime.
      let input = this.inputField;
      if (input && this._inOverflow) {
        let side = input.scrollLeft &&
                   input.scrollLeft == input.scrollLeftMax ? "start" : "end";
        this.setAttribute("textoverflow", side);
      }
    });
  }

  _getSelectedValueForClipboard() {
    // Grab the actual input field's value, not our value, which could
    // include "moz-action:".
    let inputVal = this.inputField.value;
    let selection = this.editor.selection;
    const flags = Ci.nsIDocumentEncoder.OutputPreformatted |
                  Ci.nsIDocumentEncoder.OutputRaw;
    let selectedVal = selection.toStringWithFormat("text/plain", flags, 0);

    // Handle multiple-range selection as a string for simplicity.
    if (selection.rangeCount > 1) {
      return selectedVal;
    }

    // If the selection doesn't start at the beginning or doesn't span the
    // full domain or the URL bar is modified or there is no text at all,
    // nothing else to do here.
    if (this.selectionStart > 0 || this.valueIsTyped || selectedVal == "") {
      return selectedVal;
    }

    // The selection doesn't span the full domain if it doesn't contain a slash and is
    // followed by some character other than a slash.
    if (!selectedVal.includes("/")) {
      let remainder = inputVal.replace(selectedVal, "");
      if (remainder != "" && remainder[0] != "/") {
        return selectedVal;
      }
    }

    // If the value was filled by a search suggestion, just return it.
    let action = this._parseActionUrl(this.value);
    if (action && action.type == "searchengine") {
      return selectedVal;
    }

    let uri;
    if (this.getAttribute("pageproxystate") == "valid") {
      uri = this.window.gBrowser.currentURI;
    } else {
      // We're dealing with an autocompleted value, create a new URI from that.
      try {
        uri = Services.uriFixup.createFixupURI(inputVal, Services.uriFixup.FIXUP_FLAG_NONE);
      } catch (e) {}
      if (!uri) {
        return selectedVal;
      }
    }

    uri = this.makeURIReadable(uri);

    // If the entire URL is selected, just use the actual loaded URI,
    // unless we want a decoded URI, or it's a data: or javascript: URI,
    // since those are hard to read when encoded.
    if (inputVal == selectedVal &&
        !uri.schemeIs("javascript") && !uri.schemeIs("data") &&
        !Services.prefs.getBoolPref("browser.urlbar.decodeURLsOnCopy")) {
      return uri.displaySpec;
    }

    // Just the beginning of the URL is selected, or we want a decoded
    // url. First check for a trimmed value.
    let spec = uri.displaySpec;
    let trimmedSpec = this.trimValue(spec);
    if (spec != trimmedSpec) {
      // Prepend the portion that trimValue removed from the beginning.
      // This assumes trimValue will only truncate the URL at
      // the beginning or end (or both).
      let trimmedSegments = spec.split(trimmedSpec);
      selectedVal = trimmedSegments[0] + selectedVal;
    }

    return selectedVal;
  }

  /**
   * @param {string} url
   * @returns {object}
   *   The action object
   */
  _parseActionUrl(url) {
    const MOZ_ACTION_REGEX = /^moz-action:([^,]+),(.*)$/;
    if (!MOZ_ACTION_REGEX.test(url)) {
      return null;
    }

    // URL is in the format moz-action:ACTION,PARAMS
    // Where PARAMS is a JSON encoded object.
    let [, type, params] = url.match(MOZ_ACTION_REGEX);

    let action = {
      type,
    };

    action.params = JSON.parse(params);
    for (let key in action.params) {
      action.params[key] = decodeURIComponent(action.params[key]);
    }

    if ("url" in action.params) {
      try {
        let uri = Services.io.newURI(action.params.url);
        action.params.displayUrl = this.window.losslessDecodeURI(uri);
      } catch (e) {
        action.params.displayUrl = action.params.url;
      }
    }

    return action;
  }

  // Event handlers below.

  _oninput(event) {
    this.valueIsTyped = true;

    // XXX Fill in lastKey & maxResults, and add anything else we need.
    this.controller.handleQuery(new QueryContext({
      searchString: event.target.value,
      lastKey: "",
      maxResults: 12,
      isPrivate: this.isPrivate,
    }));
  }

  _onselect(event) {
    if (!Services.clipboard.supportsSelectionClipboard()) {
      return;
    }

    if (!this.window.windowUtils.isHandlingUserInput) {
      return;
    }

    let val = this._getSelectedValueForClipboard();
    if (!val) {
      return;
    }

    Services.clipboard.copyStringToClipboard(val, Services.clipboard.kSelectionClipboard);
  }

  _onoverflow(event) {
    const targetIsPlaceholder =
      !event.originalTarget.classList.contains("anonymous-div");
    // We only care about the non-placeholder text.
    // This shouldn't be needed, see bug 1487036.
    if (targetIsPlaceholder) {
      return;
    }
    this._inOverflow = true;
    this._updateTextOverflow();
  }

  _onunderflow(event) {
    const targetIsPlaceholder =
      !event.originalTarget.classList.contains("anonymous-div");
    // We only care about the non-placeholder text.
    // This shouldn't be needed, see bug 1487036.
    if (targetIsPlaceholder) {
      return;
    }
    this._inOverflow = false;
    this._updateTextOverflow();
  }

  _onscrollend(event) {
    this._updateTextOverflow();
  }
}

/**
 * Handles copy and cut commands for the urlbar.
 */
class CopyCutController {
  /**
   * @param {UrlbarInput} urlbar
   *   The UrlbarInput instance to use this controller for.
   */
  constructor(urlbar) {
    this.urlbar = urlbar;
  }

  /**
   * @param {string} command
   *   The name of the command to handle.
   */
  doCommand(command) {
    let urlbar = this.urlbar;
    let val = urlbar._getSelectedValueForClipboard();
    if (!val) {
      return;
    }

    if (command == "cmd_cut" && this.isCommandEnabled(command)) {
      let start = urlbar.selectionStart;
      let end = urlbar.selectionEnd;
      urlbar.inputField.value = urlbar.inputField.value.substring(0, start) +
                                urlbar.inputField.value.substring(end);
      urlbar.selectionStart = urlbar.selectionEnd = start;

      let event = urlbar.window.document.createEvent("UIEvents");
      event.initUIEvent("input", true, false, this.window, 0);
      urlbar.dispatchEvent(event);

      urlbar.window.SetPageProxyState("invalid");
    }

    Cc["@mozilla.org/widget/clipboardhelper;1"]
      .getService(Ci.nsIClipboardHelper)
      .copyString(val);
  }

  /**
   * @param {string} command
   * @returns {boolean}
   *   Whether the command is handled by this controller.
   */
  supportsCommand(command) {
    switch (command) {
      case "cmd_copy":
      case "cmd_cut":
        return true;
    }
    return false;
  }

  /**
   * @param {string} command
   * @returns {boolean}
   *   Whether the command should be enabled.
   */
  isCommandEnabled(command) {
    return this.supportsCommand(command) &&
           (command != "cmd_cut" || !this.urlbar.readOnly) &&
           this.urlbar.selectionStart < this.urlbar.selectionEnd;
  }

  onEvent() {}
}

