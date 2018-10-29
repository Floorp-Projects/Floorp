/* This Source Code Form is subject to the terms of the Mozilla Public
  * License, v. 2.0. If a copy of the MPL was not distributed with this
  * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* eslint-env mozilla/browser-window */
/* globals XULCommandEvent */

// This is loaded into chrome windows with the subscript loader. Wrap in
// a block to prevent accidentally leaking globals onto `window`.
{

const inheritsMap = {
  ".searchbar-textbox": ["disabled", "disableautocomplete", "searchengine", "src", "newlines"],
  ".searchbar-search-button": ["addengines"],
};

function inheritAttribute(parent, child, attr) {
  if (!parent.hasAttribute(attr)) {
    child.removeAttribute(attr);
  } else {
    child.setAttribute(attr, parent.getAttribute(attr));
  }
}

class MozSearchbar extends MozXULElement {

  static get observedAttributes() {
    let unique = new Set();
    for (var i in inheritsMap) {
      inheritsMap[i].forEach(attr => unique.add(attr));
    }
    return Array.from(unique);
  }

  attributeChangedCallback() {
    this.inheritAttributes();
  }

  inheritAttributes() {
    if (!this.isConnected) {
      return;
    }

    for (let sel in inheritsMap) {
      let node = this.querySelector(sel);
      for (let attr of inheritsMap[sel]) {
        inheritAttribute(this, node, attr);
      }
    }
  }

  constructor() {
    super();
    this.destroy = this.destroy.bind(this);
    this._setupEventListeners();
    let searchbar = this;
    this.observer = {
      observe(aEngine, aTopic, aVerb) {
        if (aTopic == "browser-search-engine-modified" ||
          (aTopic == "browser-search-service" && aVerb == "init-complete")) {
          // Make sure the engine list is refetched next time it's needed
          searchbar._engines = null;

          // Update the popup header and update the display after any modification.
          searchbar._textbox.popup.updateHeader();
          searchbar.updateDisplay();
        }
      },
      QueryInterface: ChromeUtils.generateQI([Ci.nsIObserver]),
    };
    this.content = MozXULElement.parseXULToFragment(`
      <stringbundle src="chrome://browser/locale/search.properties"></stringbundle>
      <textbox class="searchbar-textbox" type="autocomplete" inputtype="search" placeholder="&searchInput.placeholder;" flex="1" autocompletepopup="PopupSearchAutoComplete" autocompletesearch="search-autocomplete" autocompletesearchparam="searchbar-history" maxrows="10" completeselectedindex="true" minresultsforpopup="0" inherits="disabled,disableautocomplete,searchengine,src,newlines">
        <box>
          <hbox class="searchbar-search-button" inherits="addengines" tooltiptext="&searchIcon.tooltip;">
            <image class="searchbar-search-icon"></image>
            <image class="searchbar-search-icon-overlay"></image>
          </hbox>
        </box>
        <hbox class="search-go-container">
          <image class="search-go-button urlbar-icon" hidden="true" onclick="handleSearchCommand(event);" tooltiptext="&contentSearchSubmit.tooltip;"></image>
        </hbox>
      </textbox>
    `, ["chrome://browser/locale/browser.dtd"]);
  }

  connectedCallback() {
    // Don't initialize if this isn't going to be visible
    if (this.closest("#BrowserToolbarPalette")) {
      return;
    }

    this.appendChild(document.importNode(this.content, true));
    this.inheritAttributes();
    window.addEventListener("unload", this.destroy);
    this._ignoreFocus = false;

    this._clickClosedPopup = false;

    this._stringBundle = this.querySelector("stringbundle");

    this._textboxInitialized = false;

    this._textbox = this.querySelector(".searchbar-textbox");

    this._engines = null;

    this.FormHistory = (ChromeUtils.import("resource://gre/modules/FormHistory.jsm", {})).FormHistory;

    if (this.parentNode.parentNode.localName == "toolbarpaletteitem")
      return;

    Services.obs.addObserver(this.observer, "browser-search-engine-modified");
    Services.obs.addObserver(this.observer, "browser-search-service");

    this._initialized = true;

    (window.delayedStartupPromise || Promise.resolve()).then(() => {
      window.requestIdleCallback(() => {
        Services.search.init(aStatus => {
          // Bail out if the binding's been destroyed
          if (!this._initialized)
            return;

          if (Components.isSuccessCode(aStatus)) {
            // Refresh the display (updating icon, etc)
            this.updateDisplay();
            BrowserSearch.updateOpenSearchBadge();
          } else {
            Cu.reportError("Cannot initialize search service, bailing out: " + aStatus);
          }
        });
      });
    });

    // Wait until the popupshowing event to avoid forcing immediate
    // attachment of the search-one-offs binding.
    this.textbox.popup.addEventListener("popupshowing", () => {
      let oneOffButtons = this.textbox.popup.oneOffButtons;
      // Some accessibility tests create their own <searchbar> that doesn't
      // use the popup binding below, so null-check oneOffButtons.
      if (oneOffButtons) {
        oneOffButtons.telemetryOrigin = "searchbar";
        // Set .textbox first, since the popup setter will cause
        // a _rebuild call that uses it.
        oneOffButtons.textbox = this.textbox;
        oneOffButtons.popup = this.textbox.popup;
      }
    }, { capture: true, once: true });
  }

  get engines() {
    if (!this._engines)
      this._engines = Services.search.getVisibleEngines();
    return this._engines;
  }

  set currentEngine(val) {
    Services.search.defaultEngine = val;
    return val;
  }

  get currentEngine() {
    var currentEngine = Services.search.defaultEngine;
    // Return a dummy engine if there is no currentEngine
    return currentEngine || { name: "", uri: null };
  }
  /**
   * textbox is used by sanitize.js to clear the undo history when
   * clearing form information.
   */
  get textbox() {
    return this._textbox;
  }

  set value(val) {
    return this._textbox.value = val;
  }

  get value() {
    return this._textbox.value;
  }

  destroy() {
    if (this._initialized) {
      this._initialized = false;
      window.removeEventListener("unload", this.destroy);

      Services.obs.removeObserver(this.observer, "browser-search-engine-modified");
      Services.obs.removeObserver(this.observer, "browser-search-service");
    }

    // Make sure to break the cycle from _textbox to us. Otherwise we leak
    // the world. But make sure it's actually pointing to us.
    // Also make sure the textbox has ever been constructed, otherwise the
    // _textbox getter will cause the textbox constructor to run, add an
    // observer, and leak the world too.
    if (this._textboxInitialized && this._textbox.mController.input == this)
      this._textbox.mController.input = null;
  }

  focus() {
    this._textbox.focus();
  }

  select() {
    this._textbox.select();
  }

  setIcon(element, uri) {
    element.setAttribute("src", uri);
  }

  updateDisplay() {
    var uri = this.currentEngine.iconURI;
    this.setIcon(this, uri ? uri.spec : "");

    var name = this.currentEngine.name;
    var text = this._stringBundle.getFormattedString("searchtip", [name]);
    this._textbox.label = text;
    this._textbox.tooltipText = text;
  }

  updateGoButtonVisibility() {
    this.querySelector(".search-go-button").hidden = !this._textbox.value;
  }

  openSuggestionsPanel(aShowOnlySettingsIfEmpty) {
    if (this._textbox.open)
      return;

    this._textbox.showHistoryPopup();

    if (this._textbox.value) {
      // showHistoryPopup does a startSearch("") call, ensure the
      // controller handles the text from the input box instead:
      this._textbox.mController.handleText();
    } else if (aShowOnlySettingsIfEmpty) {
      this.setAttribute("showonlysettings", "true");
    }
  }

  selectEngine(aEvent, isNextEngine) {
    // Find the new index
    var newIndex = this.engines.indexOf(this.currentEngine);
    newIndex += isNextEngine ? 1 : -1;

    if (newIndex >= 0 && newIndex < this.engines.length) {
      this.currentEngine = this.engines[newIndex];
    }

    aEvent.preventDefault();
    aEvent.stopPropagation();

    this.openSuggestionsPanel();
  }

  handleSearchCommand(aEvent, aEngine, aForceNewTab) {
    var where = "current";
    let params;

    // Open ctrl/cmd clicks on one-off buttons in a new background tab.
    if (aEvent && aEvent.originalTarget.classList.contains("search-go-button")) {
      if (aEvent.button == 2)
        return;
      where = whereToOpenLink(aEvent, false, true);
    } else if (aForceNewTab) {
      where = "tab";
      if (Services.prefs.getBoolPref("browser.tabs.loadInBackground"))
        where += "-background";
    } else {
      var newTabPref = Services.prefs.getBoolPref("browser.search.openintab");
      if (((aEvent instanceof KeyboardEvent && aEvent.altKey) ^ newTabPref) &&
        !gBrowser.selectedTab.isEmpty) {
        where = "tab";
      }
      if ((aEvent instanceof MouseEvent) &&
        (aEvent.button == 1 || aEvent.getModifierState("Accel"))) {
        where = "tab";
        params = {
          inBackground: true,
        };
      }
    }

    this.handleSearchCommandWhere(aEvent, aEngine, where, params);
  }

  handleSearchCommandWhere(aEvent, aEngine, aWhere, aParams) {
    var textBox = this._textbox;
    var textValue = textBox.value;

    let selection = this.telemetrySearchDetails;
    let oneOffRecorded = false;

    BrowserUsageTelemetry.recordSearchbarSelectedResultMethod(
      aEvent,
      selection ? selection.index : -1
    );

    if (!selection || (selection.index == -1)) {
      oneOffRecorded = this.textbox.popup.oneOffButtons
        .maybeRecordTelemetry(aEvent, aWhere, aParams);
      if (!oneOffRecorded) {
        let source = "unknown";
        let type = "unknown";
        let target = aEvent.originalTarget;
        if (aEvent instanceof KeyboardEvent) {
          type = "key";
        } else if (aEvent instanceof MouseEvent) {
          type = "mouse";
          if (target.classList.contains("search-panel-header") ||
            target.parentNode.classList.contains("search-panel-header")) {
            source = "header";
          }
        } else if (aEvent instanceof XULCommandEvent) {
          if (target.getAttribute("anonid") == "paste-and-search") {
            source = "paste";
          }
        }
        if (!aEngine) {
          aEngine = this.currentEngine;
        }
        BrowserSearch.recordOneoffSearchInTelemetry(aEngine, source, type,
          aWhere);
      }
    }

    // This is a one-off search only if oneOffRecorded is true.
    this.doSearch(textValue, aWhere, aEngine, aParams, oneOffRecorded);

    if (aWhere == "tab" && aParams && aParams.inBackground)
      this.focus();
  }

  doSearch(aData, aWhere, aEngine, aParams, aOneOff) {
    var textBox = this._textbox;

    // Save the current value in the form history
    if (aData && !PrivateBrowsingUtils.isWindowPrivate(window) && this.FormHistory.enabled) {
      this.FormHistory.update({
        op: "bump",
        fieldname: textBox.getAttribute("autocompletesearchparam"),
        value: aData,
      }, {
        handleError(aError) {
          Cu.reportError("Saving search to form history failed: " + aError.message);
        },
      });
    }

    let engine = aEngine || this.currentEngine;
    var submission = engine.getSubmission(aData, null, "searchbar");
    let telemetrySearchDetails = this.telemetrySearchDetails;
    this.telemetrySearchDetails = null;
    if (telemetrySearchDetails && telemetrySearchDetails.index == -1) {
      telemetrySearchDetails = null;
    }
    // If we hit here, we come either from a one-off, a plain search or a suggestion.
    const details = {
      isOneOff: aOneOff,
      isSuggestion: (!aOneOff && telemetrySearchDetails),
      selection: telemetrySearchDetails,
    };
    BrowserSearch.recordSearchInTelemetry(engine, "searchbar", details);
    // null parameter below specifies HTML response for search
    let params = {
      postData: submission.postData,
    };
    if (aParams) {
      for (let key in aParams) {
        params[key] = aParams[key];
      }
    }
    openTrustedLinkIn(submission.uri.spec, aWhere, params);
  }

  disconnectedCallback() {
    this.destroy();
    while (this.firstChild) {
      this.firstChild.remove();
    }
  }

  _setupEventListeners() {
    this.addEventListener("command", (event) => {
      const target = event.originalTarget;
      if (target.engine) {
        this.currentEngine = target.engine;
      } else if (target.classList.contains("addengine-item")) {
        // Select the installed engine if the installation succeeds
        var installCallback = {
          onSuccess: engine => this.currentEngine = engine,
        };
        Services.search.addEngine(target.getAttribute("uri"), null,
          target.getAttribute("src"), false,
          installCallback);
      } else
        return;

      this.focus();
      this.select();
    });

    this.addEventListener("DOMMouseScroll", (event) => {
      if (event.getModifierState("Accel")) {
        this.selectEngine(event, event.detail > 0);
      }
    }, true);

    this.addEventListener("input", (event) => { this.updateGoButtonVisibility(); });

    this.addEventListener("drop", (event) => { this.updateGoButtonVisibility(); });

    this.addEventListener("blur", (event) => {
      // If the input field is still focused then a different window has
      // received focus, ignore the next focus event.
      this._ignoreFocus = (document.activeElement == this._textbox.inputField);
    }, true);

    this.addEventListener("focus", (event) => {
      // Speculatively connect to the current engine's search URI (and
      // suggest URI, if different) to reduce request latency
      this.currentEngine.speculativeConnect({
        window,
        originAttributes: gBrowser.contentPrincipal
          .originAttributes,
      });

      if (this._ignoreFocus) {
        // This window has been re-focused, don't show the suggestions
        this._ignoreFocus = false;
        return;
      }

      // Don't open the suggestions if there is no text in the textbox.
      if (!this._textbox.value)
        return;

      // Don't open the suggestions if the mouse was used to focus the
      // textbox, that will be taken care of in the click handler.
      if (Services.focus.getLastFocusMethod(window) & Services.focus.FLAG_BYMOUSE)
        return;

      this.openSuggestionsPanel();
    }, true);

    this.addEventListener("mousedown", (event) => {
      if (event.originalTarget.classList.contains("searchbar-search-button")) {
        this._clickClosedPopup = this._textbox.popup._isHiding;
      }
    }, true);

    this.addEventListener("mousedown", (event) => {
      // Ignore right clicks
      if (event.button != 0) {
        return;
      }

      // Ignore clicks on the search go button.
      if (event.originalTarget.classList.contains("search-go-button")) {
        return;
      }

      let isIconClick = event.originalTarget.classList.contains("searchbar-search-button");

      // Ignore clicks on the icon if they were made to close the popup
      if (isIconClick && this._clickClosedPopup) {
        return;
      }

      // Open the suggestions whenever clicking on the search icon or if there
      // is text in the textbox.
      if (isIconClick || this._textbox.value) {
        this.openSuggestionsPanel(true);
      }
    });

  }
}

customElements.define("searchbar", MozSearchbar);

}
