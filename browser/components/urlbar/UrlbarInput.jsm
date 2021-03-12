/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["UrlbarInput"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  AppConstants: "resource://gre/modules/AppConstants.jsm",
  BrowserSearchTelemetry: "resource:///modules/BrowserSearchTelemetry.jsm",
  BrowserUIUtils: "resource:///modules/BrowserUIUtils.jsm",
  CONTEXTUAL_SERVICES_PING_TYPES:
    "resource:///modules/PartnerLinkAttribution.jsm",
  ExtensionSearchHandler: "resource://gre/modules/ExtensionSearchHandler.jsm",
  ObjectUtils: "resource://gre/modules/ObjectUtils.jsm",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.jsm",
  PromiseUtils: "resource://gre/modules/PromiseUtils.jsm",
  ReaderMode: "resource://gre/modules/ReaderMode.jsm",
  PartnerLinkAttribution: "resource:///modules/PartnerLinkAttribution.jsm",
  SearchUIUtils: "resource:///modules/SearchUIUtils.jsm",
  SearchUtils: "resource://gre/modules/SearchUtils.jsm",
  Services: "resource://gre/modules/Services.jsm",
  UrlbarController: "resource:///modules/UrlbarController.jsm",
  UrlbarEventBufferer: "resource:///modules/UrlbarEventBufferer.jsm",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.jsm",
  UrlbarProvidersManager: "resource:///modules/UrlbarProvidersManager.jsm",
  UrlbarQueryContext: "resource:///modules/UrlbarUtils.jsm",
  UrlbarSearchUtils: "resource:///modules/UrlbarSearchUtils.jsm",
  UrlbarTokenizer: "resource:///modules/UrlbarTokenizer.jsm",
  UrlbarUtils: "resource:///modules/UrlbarUtils.jsm",
  UrlbarValueFormatter: "resource:///modules/UrlbarValueFormatter.jsm",
  UrlbarView: "resource:///modules/UrlbarView.jsm",
});

XPCOMUtils.defineLazyServiceGetter(
  this,
  "ClipboardHelper",
  "@mozilla.org/widget/clipboardhelper;1",
  "nsIClipboardHelper"
);

const DEFAULT_FORM_HISTORY_NAME = "searchbar-history";
const SEARCH_BUTTON_ID = "urlbar-search-button";

// The scalar category of TopSites click for Contextual Services
const SCALAR_CATEGORY_TOPSITES = "contextual.services.topsites.click";

let getBoundsWithoutFlushing = element =>
  element.ownerGlobal.windowUtils.getBoundsWithoutFlushing(element);
let px = number => number.toFixed(2) + "px";

/**
 * Implements the text input part of the address bar UI.
 */
class UrlbarInput {
  /**
   * @param {object} options
   *   The initial options for UrlbarInput.
   * @param {object} options.textbox
   *   The container element.
   */
  constructor(options = {}) {
    this.textbox = options.textbox;

    this.window = this.textbox.ownerGlobal;
    this.isPrivate = PrivateBrowsingUtils.isWindowPrivate(this.window);
    this.document = this.window.document;

    // Create the panel to contain results.
    this.textbox.appendChild(
      this.window.MozXULElement.parseXULToFragment(`
        <vbox class="urlbarView"
              role="group"
              tooltip="aHTMLTooltip">
          <html:div class="urlbarView-body-outer">
            <html:div class="urlbarView-body-inner">
              <html:div id="urlbar-results"
                        class="urlbarView-results"
                        role="listbox"/>
            </html:div>
          </html:div>
          <hbox class="search-one-offs"
                compact="true"
                includecurrentengine="true"
                disabletab="true"/>
        </vbox>
      `)
    );
    this.panel = this.textbox.querySelector(".urlbarView");

    this.searchButton = UrlbarPrefs.get("experimental.searchButton");
    if (this.searchButton) {
      this.textbox.classList.add("searchButton");
    }

    this.controller = new UrlbarController({
      input: this,
      eventTelemetryCategory: options.eventTelemetryCategory,
    });
    this.view = new UrlbarView(this);
    this.valueIsTyped = false;
    this.formHistoryName = DEFAULT_FORM_HISTORY_NAME;
    this.lastQueryContextPromise = Promise.resolve();
    this._actionOverrideKeyCount = 0;
    this._autofillPlaceholder = "";
    this._lastSearchString = "";
    this._lastValidURLStr = "";
    this._valueOnLastSearch = "";
    this._resultForCurrentValue = null;
    this._suppressStartQuery = false;
    this._suppressPrimaryAdjustment = false;
    this._untrimmedValue = "";

    // Search modes are per browser and are stored in this map.  For a
    // browser, search mode can be in preview mode, confirmed, or both.
    // Typically, search mode is entered in preview mode with a particular
    // source and is confirmed with the same source once a query starts.  It's
    // also possible for a confirmed search mode to be replaced with a preview
    // mode with a different source, and in those cases, we need to re-confirm
    // search mode when preview mode is exited. In addition, only confirmed
    // search modes should be restored across sessions. We therefore need to
    // keep track of both the current confirmed and preview modes, per browser.
    //
    // For each browser with a search mode, this maps the browser to an object
    // like this: { preview, confirmed }.  Both `preview` and `confirmed` are
    // search mode objects; see the setSearchMode documentation.  Either one may
    // be undefined if that particular mode is not active for the browser.
    this._searchModesByBrowser = new WeakMap();

    this.QueryInterface = ChromeUtils.generateQI([
      "nsIObserver",
      "nsISupportsWeakReference",
    ]);
    this._addObservers();

    // This exists only for tests.
    this._enableAutofillPlaceholder = true;

    // Forward certain methods and properties.
    const CONTAINER_METHODS = [
      "getAttribute",
      "hasAttribute",
      "querySelector",
      "setAttribute",
      "removeAttribute",
      "toggleAttribute",
    ];
    const INPUT_METHODS = ["addEventListener", "blur", "removeEventListener"];
    const READ_WRITE_PROPERTIES = [
      "placeholder",
      "readOnly",
      "selectionStart",
      "selectionEnd",
    ];

    for (let method of CONTAINER_METHODS) {
      this[method] = (...args) => {
        return this.textbox[method](...args);
      };
    }

    for (let method of INPUT_METHODS) {
      this[method] = (...args) => {
        return this.inputField[method](...args);
      };
    }

    for (let property of READ_WRITE_PROPERTIES) {
      Object.defineProperty(this, property, {
        enumerable: true,
        get() {
          return this.inputField[property];
        },
        set(val) {
          this.inputField[property] = val;
        },
      });
    }

    this.inputField = this.querySelector("#urlbar-input");
    this._inputContainer = this.querySelector("#urlbar-input-container");
    this._identityBox = this.querySelector("#identity-box");
    this._searchModeIndicator = this.querySelector(
      "#urlbar-search-mode-indicator"
    );
    this._searchModeIndicatorTitle = this._searchModeIndicator.querySelector(
      "#urlbar-search-mode-indicator-title"
    );
    this._searchModeIndicatorClose = this._searchModeIndicator.querySelector(
      "#urlbar-search-mode-indicator-close"
    );
    this._searchModeLabel = this.querySelector("#urlbar-label-search-mode");
    this._toolbar = this.textbox.closest("toolbar");

    XPCOMUtils.defineLazyGetter(this, "valueFormatter", () => {
      return new UrlbarValueFormatter(this);
    });

    // If the toolbar is not visible in this window or the urlbar is readonly,
    // we'll stop here, so that most properties of the input object are valid,
    // but we won't handle events.
    if (!this.window.toolbar.visible || this.readOnly) {
      return;
    }

    // The event bufferer can be used to defer events that may affect users
    // muscle memory; for example quickly pressing DOWN+ENTER should end up
    // on a predictable result, regardless of the search status. The event
    // bufferer will invoke the handling code at the right time.
    this.eventBufferer = new UrlbarEventBufferer(this);

    this._inputFieldEvents = [
      "compositionstart",
      "compositionend",
      "contextmenu",
      "dragover",
      "dragstart",
      "drop",
      "focus",
      "blur",
      "input",
      "keydown",
      "keyup",
      "mouseover",
      "overflow",
      "underflow",
      "paste",
      "scrollend",
      "select",
    ];
    for (let name of this._inputFieldEvents) {
      this.addEventListener(name, this);
    }

    this.window.addEventListener("mousedown", this);
    if (AppConstants.platform == "win") {
      this.window.addEventListener("draggableregionleftmousedown", this);
    }
    this.textbox.addEventListener("mousedown", this);

    // This listener handles clicks from our children too, included the search mode
    // indicator close button.
    this._inputContainer.addEventListener("click", this);

    // This is used to detect commands launched from the panel, to avoid
    // recording abandonment events when the command causes a blur event.
    this.view.panel.addEventListener("command", this, true);

    this.window.gBrowser.tabContainer.addEventListener("TabSelect", this);

    this.window.addEventListener("customizationstarting", this);
    this.window.addEventListener("aftercustomization", this);

    this.updateLayoutBreakout();

    this._initCopyCutController();
    this._initPasteAndGo();
    if (UrlbarPrefs.get("browser.proton.urlbar.enabled")) {
      this.addSearchEngineHelper = new AddSearchEngineHelper(this);
    }

    // Tracks IME composition.
    this._compositionState = UrlbarUtils.COMPOSITION.NONE;
    this._compositionClosedPopup = false;

    this.editor.newlineHandling =
      Ci.nsIEditor.eNewlinesStripSurroundingWhitespace;
  }

  /**
   * Applies styling to the text in the urlbar input, depending on the text.
   */
  formatValue() {
    // The editor may not exist if the toolbar is not visible.
    if (this.editor) {
      this.valueFormatter.update();
    }
  }

  focus() {
    let beforeFocus = new CustomEvent("beforefocus", {
      bubbles: true,
      cancelable: true,
    });
    this.inputField.dispatchEvent(beforeFocus);
    if (beforeFocus.defaultPrevented) {
      return;
    }

    this.inputField.focus();
  }

  select() {
    let beforeSelect = new CustomEvent("beforeselect", {
      bubbles: true,
      cancelable: true,
    });
    this.inputField.dispatchEvent(beforeSelect);
    if (beforeSelect.defaultPrevented) {
      return;
    }

    // See _on_select().  HTMLInputElement.select() dispatches a "select"
    // event but does not set the primary selection.
    this._suppressPrimaryAdjustment = true;
    this.inputField.select();
    this._suppressPrimaryAdjustment = false;
  }

  /**
   * Sets the URI to display in the location bar.
   *
   * @param {nsIURI} [uri]
   *        If this is unspecified, the current URI will be used.
   * @param {boolean} [dueToTabSwitch]
   *        True if this is being called due to switching tabs and false
   *        otherwise.
   */
  setURI(uri = null, dueToTabSwitch = false) {
    let value = this.window.gBrowser.userTypedValue;
    let valid = false;

    // Explicitly check for nulled out value. We don't want to reset the URL
    // bar if the user has deleted the URL and we'd just put the same URL
    // back. See bug 304198.
    if (value === null) {
      uri = uri || this.window.gBrowser.currentURI;
      // Strip off usernames and passwords for the location bar
      try {
        uri = Services.io.createExposableURI(uri);
      } catch (e) {}

      // Replace initial page URIs with an empty string
      // only if there's no opener (bug 370555).
      if (
        this.window.isInitialPage(uri) &&
        BrowserUIUtils.checkEmptyPageOrigin(
          this.window.gBrowser.selectedBrowser,
          uri
        )
      ) {
        value = "";
      } else {
        // We should deal with losslessDecodeURI throwing for exotic URIs
        try {
          value = losslessDecodeURI(uri);
        } catch (ex) {
          value = "about:blank";
        }
      }

      valid =
        !this.window.isBlankPageURL(uri.spec) || uri.schemeIs("moz-extension");
    } else if (
      this.window.isInitialPage(value) &&
      BrowserUIUtils.checkEmptyPageOrigin(this.window.gBrowser.selectedBrowser)
    ) {
      value = "";
      valid = true;
    }

    let isDifferentValidValue = valid && value != this.untrimmedValue;
    this.value = value;
    this.valueIsTyped = !valid;
    this.removeAttribute("usertyping");
    if (isDifferentValidValue) {
      // The selection is enforced only for new values, to avoid overriding the
      // cursor position when the user switches windows while typing.
      this.selectionStart = this.selectionEnd = 0;
    }

    // The proxystate must be set before setting search mode below because
    // search mode depends on it.
    this.setPageProxyState(valid ? "valid" : "invalid", dueToTabSwitch);

    // If we're switching tabs, restore the tab's search mode.  Otherwise, if
    // the URI is valid, exit search mode.  This must happen after setting
    // proxystate above because search mode depends on it.
    if (dueToTabSwitch && !valid) {
      this.restoreSearchModeState();
    } else if (valid) {
      this.searchMode = null;
    }
  }

  /**
   * Converts an internal URI (e.g. a URI with a username or password) into one
   * which we can expose to the user.
   *
   * @param {nsIURI} uri
   *   The URI to be converted
   * @returns {nsIURI}
   *   The converted, exposable URI
   */
  makeURIReadable(uri) {
    // Avoid copying 'about:reader?url=', and always provide the original URI:
    // Reader mode ensures we call createExposableURI itself.
    let readerStrippedURI = ReaderMode.getOriginalUrlObjectForDisplay(
      uri.displaySpec
    );
    if (readerStrippedURI) {
      return readerStrippedURI;
    }

    try {
      return Services.io.createExposableURI(uri);
    } catch (ex) {}

    return uri;
  }

  /**
   * Passes DOM events to the _on_<event type> methods.
   * @param {Event} event
   */
  handleEvent(event) {
    let methodName = "_on_" + event.type;
    if (methodName in this) {
      this[methodName](event);
    } else {
      throw new Error("Unrecognized UrlbarInput event: " + event.type);
    }
  }

  /**
   * Handles an event which might open text or a URL. If the event requires
   * doing so, handleCommand forwards it to handleNavigation.
   *
   * @param {Event} [event] The event triggering the open.
   */
  handleCommand(event = null) {
    let isMouseEvent = event instanceof this.window.MouseEvent;
    if (isMouseEvent && event.button == 2) {
      // Do nothing for right clicks.
      return;
    }

    // Determine whether to use the selected one-off search button.  In
    // one-off search buttons parlance, "selected" means that the button
    // has been navigated to via the keyboard.  So we want to use it if
    // the triggering event is not a mouse click -- i.e., it's a Return
    // key -- or if the one-off was mouse-clicked.
    if (this.view.isOpen) {
      let selectedOneOff = this.view.oneOffSearchButtons.selectedButton;
      if (selectedOneOff && (!isMouseEvent || event.target == selectedOneOff)) {
        this.view.oneOffSearchButtons.handleSearchCommand(event, {
          engineName: selectedOneOff.engine?.name,
          source: selectedOneOff.source,
          entry: "oneoff",
        });
        return;
      }
    }

    this.handleNavigation({ event });
  }

  /**
   * Handles an event which would cause a URL or text to be opened.
   *
   * @param {Event} [event]
   *   The event triggering the open.
   * @param {object} [oneOffParams]
   *   Optional. Pass if this navigation was triggered by a one-off. Practically
   *   speaking, UrlbarSearchOneOffs passes this when the user holds certain key
   *   modifiers while picking a one-off. In those cases, we do an immediate
   *   search using the one-off's engine instead of entering search mode.
   * @param {string} oneOffParams.openWhere
   *   Where we expect the result to be opened.
   * @param {object} oneOffParams.openParams
   *   The parameters related to where the result will be opened.
   * @param {Node} oneOffParams.engine
   *   The selected one-off's engine.
   * @param {object} [triggeringPrincipal]
   *   The principal that the action was triggered from.
   */
  handleNavigation({ event, oneOffParams, triggeringPrincipal }) {
    let element = this.view.selectedElement;
    let result = this.view.getResultFromElement(element);
    let openParams = oneOffParams?.openParams || {};

    // If the value was submitted during composition, the result may not have
    // been updated yet, because the input event happens after composition end.
    // We can't trust element nor _resultForCurrentValue targets in that case,
    // so we'always generate a new heuristic to load.
    let isComposing = this.editor.composing;

    // Use the selected element if we have one; this is usually the case
    // when the view is open.
    let selectedPrivateResult =
      result &&
      result.type == UrlbarUtils.RESULT_TYPE.SEARCH &&
      result.payload.inPrivateWindow;
    let selectedPrivateEngineResult =
      selectedPrivateResult && result.payload.isPrivateEngine;
    if (
      !isComposing &&
      element &&
      (!oneOffParams?.engine || selectedPrivateEngineResult)
    ) {
      this.pickElement(element, event);
      return;
    }

    // We don't select a heuristic result when we're autofilling a token alias,
    // but we want pressing Enter to behave like the first result was selected.
    if (!result && this.value.startsWith("@")) {
      let tokenAliasResult = this.view.getResultAtIndex(0);
      if (tokenAliasResult?.autofill && tokenAliasResult?.payload.keyword) {
        this.pickResult(tokenAliasResult, event);
        return;
      }
    }

    let url;
    let selType = this.controller.engagementEvent.typeFromElement(element);
    let typedValue = this.value;
    if (oneOffParams?.engine) {
      selType = "oneoff";
      typedValue = this._lastSearchString;
      // If there's a selected one-off button then load a search using
      // the button's engine.
      result = this._resultForCurrentValue;
      let searchString =
        (result && (result.payload.suggestion || result.payload.query)) ||
        this._lastSearchString;
      [url, openParams.postData] = UrlbarUtils.getSearchQueryUrl(
        oneOffParams.engine,
        searchString
      );
      this._recordSearch(oneOffParams.engine, event, { url });

      UrlbarUtils.addToFormHistory(
        this,
        searchString,
        oneOffParams.engine.name
      ).catch(Cu.reportError);
    } else {
      // Use the current value if we don't have a UrlbarResult e.g. because the
      // view is closed.
      url = this.untrimmedValue;
      openParams.postData = null;
    }

    if (!url) {
      return;
    }

    // When the user hits enter in a local search mode and there's no selected
    // result or one-off, don't do anything.
    if (
      this.searchMode &&
      !this.searchMode.engineName &&
      !result &&
      !oneOffParams
    ) {
      return;
    }

    let selectedResult = result || this.view.selectedResult;
    this.controller.recordSelectedResult(event, selectedResult);

    let where = oneOffParams?.openWhere || this._whereToOpen(event);
    if (selectedPrivateResult) {
      where = "window";
      openParams.private = true;
    }
    openParams.allowInheritPrincipal = false;
    url = this._maybeCanonizeURL(event, url) || url.trim();

    this.controller.engagementEvent.record(event, {
      searchString: typedValue,
      selIndex: this.view.selectedRowIndex,
      selType,
      provider: selectedResult?.providerName,
    });

    let isValidUrl = false;
    try {
      new URL(url);
      isValidUrl = true;
    } catch (ex) {}
    if (isValidUrl) {
      this._loadURL(url, event, where, openParams);
      return;
    }

    // This is not a URL and there's no selected element, because likely the
    // view is closed, or paste&go was used.
    // We must act consistently here, having or not an open view should not
    // make a difference if the search string is the same.

    // If we have a result for the current value, we can just use it.
    if (!isComposing && this._resultForCurrentValue) {
      this.pickResult(this._resultForCurrentValue, event);
      return;
    }

    // Otherwise, we must fetch the heuristic result for the current value.
    // TODO (Bug 1604927): If the urlbar results are restricted to a specific
    // engine, here we must search with that specific engine; indeed the
    // docshell wouldn't know about our engine restriction.
    // Also remember to invoke this._recordSearch, after replacing url with
    // the appropriate engine submission url.
    let browser = this.window.gBrowser.selectedBrowser;
    let lastLocationChange = browser.lastLocationChange;
    UrlbarUtils.getHeuristicResultFor(url)
      .then(newResult => {
        // Because this happens asynchronously, we must verify that the browser
        // location did not change in the meanwhile.
        if (
          where != "current" ||
          browser.lastLocationChange == lastLocationChange
        ) {
          this.pickResult(newResult, event, null, browser);
        }
      })
      .catch(ex => {
        if (url) {
          // Something went wrong, we should always have a heuristic result,
          // otherwise it means we're not able to search at all, maybe because
          // some parts of the profile are corrupt.
          // The urlbar should still allow to search or visit the typed string,
          // so that the user can look for help to resolve the problem.
          let flags =
            Ci.nsIURIFixup.FIXUP_FLAG_FIX_SCHEME_TYPOS |
            Ci.nsIURIFixup.FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP;
          if (this.isPrivate) {
            flags |= Ci.nsIURIFixup.FIXUP_FLAG_PRIVATE_CONTEXT;
          }
          let {
            preferredURI: uri,
            postData,
          } = Services.uriFixup.getFixupURIInfo(url, flags);
          if (
            where != "current" ||
            browser.lastLocationChange == lastLocationChange
          ) {
            openParams.postData = postData;
            this._loadURL(uri.spec, event, where, openParams, null, browser);
          }
        }
      });
    // Don't add further handling here, the catch above is our last resort.
  }

  handleRevert() {
    this.window.gBrowser.userTypedValue = null;
    // Nullify search mode before setURI so it won't try to restore it.
    this.searchMode = null;
    this.setURI(null, true);
    if (this.value && this.focused) {
      this.select();
    }
  }

  /**
   * Called when an element of the view is picked.
   *
   * @param {Element} element The element that was picked.
   * @param {Event} event The event that picked the element.
   */
  pickElement(element, event) {
    let result = this.view.getResultFromElement(element);
    if (!result) {
      return;
    }
    this.pickResult(result, event, element);
  }

  /**
   * Called when a result is picked.
   *
   * @param {UrlbarResult} result The result that was picked.
   * @param {Event} event The event that picked the result.
   * @param {DOMElement} element the picked view element, if available.
   * @param {object} browser The browser to use for the load.
   */
  pickResult(
    result,
    event,
    element = null,
    browser = this.window.gBrowser.selectedBrowser
  ) {
    // When a one-off is selected, we restyle heuristic results to look like
    // search results. In the unlikely event that they are clicked, instead of
    // picking the results as usual, we confirm search mode, same as if the user
    // had selected them and pressed the enter key. Restyling results in this
    // manner was agreed on as a compromise between consistent UX and
    // engineering effort. See review discussion at bug 1667766.
    if (
      result.heuristic &&
      this.searchMode?.isPreview &&
      this.view.oneOffSearchButtons.selectedButton
    ) {
      this.confirmSearchMode();
      this.search(this.value);
      return;
    }

    let urlOverride;
    if (element?.classList.contains("urlbarView-help")) {
      urlOverride = result.payload.helpUrl;
    }

    let originalUntrimmedValue = this.untrimmedValue;
    let isCanonized = this.setValueFromResult({ result, event, urlOverride });
    let where = this._whereToOpen(event);
    let openParams = {
      allowInheritPrincipal: false,
    };

    if (
      urlOverride &&
      result.type != UrlbarUtils.RESULT_TYPE.TIP &&
      where == "current"
    ) {
      // Open non-tip help links in a new tab unless the user held a modifier.
      // TODO (bug 1696232): Do this for tip help links, too.
      where = "tab";
    }

    let selIndex = result.rowIndex;
    if (!result.payload.providesSearchMode) {
      this.view.close({ elementPicked: true });
    }

    this.controller.recordSelectedResult(event, result);

    if (isCanonized) {
      this.controller.engagementEvent.record(event, {
        searchString: this._lastSearchString,
        selIndex,
        selType: "canonized",
        provider: result.providerName,
      });
      this._loadURL(this.value, event, where, openParams, browser);
      return;
    }

    let { url, postData } = urlOverride
      ? { url: urlOverride, postData: null }
      : UrlbarUtils.getUrlFromResult(result);
    openParams.postData = postData;

    switch (result.type) {
      case UrlbarUtils.RESULT_TYPE.URL: {
        // Bug 1578856: both the provider and the docshell run heuristics to
        // decide how to handle a non-url string, either fixing it to a url, or
        // searching for it.
        // Some preferences can control the docshell behavior, for example
        // if dns_first_for_single_words is true, the docshell looks up the word
        // against the dns server, and either loads it as an url or searches for
        // it, depending on the lookup result. The provider instead will always
        // return a fixed url in this case, because URIFixup is synchronous and
        // can't do a synchronous dns lookup. A possible long term solution
        // would involve sharing the docshell logic with the provider, along
        // with the dns lookup.
        // For now, in this specific case, we'll override the result's url
        // with the input value, and let it pass through to _loadURL(), and
        // finally to the docshell.
        // This also means that in some cases the heuristic result will show a
        // Visit entry, but the docshell will instead execute a search. It's a
        // rare case anyway, most likely to happen for enterprises customizing
        // the urifixup prefs.
        if (
          result.heuristic &&
          UrlbarPrefs.get("browser.fixup.dns_first_for_single_words") &&
          UrlbarUtils.looksLikeSingleWordHost(originalUntrimmedValue)
        ) {
          url = originalUntrimmedValue;
        }
        break;
      }
      case UrlbarUtils.RESULT_TYPE.KEYWORD: {
        // If this result comes from a bookmark keyword, let it inherit the
        // current document's principal, otherwise bookmarklets would break.
        openParams.allowInheritPrincipal = true;
        break;
      }
      case UrlbarUtils.RESULT_TYPE.TAB_SWITCH: {
        if (this.hasAttribute("actionoverride")) {
          where = "current";
          break;
        }

        this.handleRevert();
        let prevTab = this.window.gBrowser.selectedTab;
        let loadOpts = {
          adoptIntoActiveWindow: UrlbarPrefs.get(
            "switchTabs.adoptIntoActiveWindow"
          ),
        };

        this.controller.engagementEvent.record(event, {
          searchString: this._lastSearchString,
          selIndex,
          selType: "tabswitch",
          provider: result.providerName,
        });

        let switched = this.window.switchToTabHavingURI(
          Services.io.newURI(url),
          false,
          loadOpts
        );
        if (switched && prevTab.isEmpty) {
          this.window.gBrowser.removeTab(prevTab);
        }
        return;
      }
      case UrlbarUtils.RESULT_TYPE.SEARCH: {
        if (result.payload.providesSearchMode) {
          let searchModeParams = this._searchModeForResult(result);
          if (searchModeParams) {
            this.searchMode = searchModeParams;
            this.search("");
          }
          return;
        }

        if (
          !this.searchMode &&
          result.heuristic &&
          // If we asked the DNS earlier, avoid the post-facto check.
          !UrlbarPrefs.get("browser.fixup.dns_first_for_single_words") &&
          // TODO (bug 1642623): for now there is no smart heuristic to skip the
          // DNS lookup, so any value above 0 will run it.
          UrlbarPrefs.get("dnsResolveSingleWordsAfterSearch") > 0 &&
          this.window.gKeywordURIFixup &&
          UrlbarUtils.looksLikeSingleWordHost(originalUntrimmedValue)
        ) {
          // When fixing a single word to a search, the docShell would also
          // query the DNS and if resolved ask the user whether they would
          // rather visit that as a host. On a positive answer, it adds the host
          // to the list that we use to make decisions.
          // Because we are directly asking for a search here, bypassing the
          // docShell, we need to do the same ourselves.
          // See also URIFixupChild.jsm and keyword-uri-fixup.
          let fixupInfo = this._getURIFixupInfo(originalUntrimmedValue.trim());
          if (fixupInfo) {
            this.window.gKeywordURIFixup.check(
              this.window.gBrowser.selectedBrowser,
              fixupInfo
            );
          }
        }

        if (result.payload.inPrivateWindow) {
          where = "window";
          openParams.private = true;
        }

        const actionDetails = {
          isSuggestion: !!result.payload.suggestion,
          isFormHistory: result.source == UrlbarUtils.RESULT_SOURCE.HISTORY,
          alias: result.payload.keyword,
          url,
        };
        const engine = Services.search.getEngineByName(result.payload.engine);
        this._recordSearch(engine, event, actionDetails);

        if (!result.payload.inPrivateWindow) {
          UrlbarUtils.addToFormHistory(
            this,
            result.payload.suggestion || result.payload.query,
            engine.name
          ).catch(Cu.reportError);
        }
        break;
      }
      case UrlbarUtils.RESULT_TYPE.TIP: {
        let scalarName;
        if (element.classList.contains("urlbarView-help")) {
          url = result.payload.helpUrl;
          if (!url) {
            Cu.reportError("helpUrl not specified");
            return;
          }
          scalarName = `${result.payload.type}-help`;
        } else {
          scalarName = `${result.payload.type}-picked`;
        }
        Services.telemetry.keyedScalarAdd("urlbar.tips", scalarName, 1);
        if (!url) {
          this.handleRevert();
          this.controller.engagementEvent.record(event, {
            searchString: this._lastSearchString,
            selIndex,
            selType: "tip",
            provider: result.providerName,
          });
          let provider = UrlbarProvidersManager.getProvider(
            result.providerName
          );
          if (!provider) {
            Cu.reportError(`Provider not found: ${result.providerName}`);
            return;
          }
          provider.tryMethod("pickResult", result, element);
          return;
        }
        break;
      }
      case UrlbarUtils.RESULT_TYPE.DYNAMIC: {
        url = result.payload.url;
        // Do not revert the Urlbar if we're going to navigate. We want the URL
        // populated so we can navigate to it.
        if (!url || !result.payload.shouldNavigate) {
          this.handleRevert();
        }

        let provider = UrlbarProvidersManager.getProvider(result.providerName);
        if (!provider) {
          Cu.reportError(`Provider not found: ${result.providerName}`);
          return;
        }
        provider.tryMethod("pickResult", result, element);

        // If we won't be navigating, this is the end of the engagement.
        if (!url || !result.payload.shouldNavigate) {
          this.controller.engagementEvent.record(event, {
            selIndex,
            searchString: this._lastSearchString,
            selType: this.controller.engagementEvent.typeFromElement(element),
            provider: result.providerName,
          });
          return;
        }
        break;
      }
      case UrlbarUtils.RESULT_TYPE.OMNIBOX: {
        this.controller.engagementEvent.record(event, {
          searchString: this._lastSearchString,
          selIndex,
          selType: "extension",
          provider: result.providerName,
        });

        // The urlbar needs to revert to the loaded url when a command is
        // handled by the extension.
        this.handleRevert();
        // We don't directly handle a load when an Omnibox API result is picked,
        // instead we forward the request to the WebExtension itself, because
        // the value may not even be a url.
        // We pass the keyword and content, that actually is the retrieved value
        // prefixed by the keyword. ExtensionSearchHandler uses this keyword
        // redundancy as a sanity check.
        ExtensionSearchHandler.handleInputEntered(
          result.payload.keyword,
          result.payload.content,
          where
        );
        return;
      }
    }

    if (!url) {
      throw new Error(`Invalid url for result ${JSON.stringify(result)}`);
    }

    if (!this.isPrivate && !result.heuristic) {
      // This should not interrupt the load anyway.
      UrlbarUtils.addToInputHistory(url, this._lastSearchString).catch(
        Cu.reportError
      );
    }

    this.controller.engagementEvent.record(event, {
      searchString: this._lastSearchString,
      selIndex,
      selType: this.controller.engagementEvent.typeFromElement(element),
      provider: result.providerName,
    });

    if (result.payload.sendAttributionRequest) {
      PartnerLinkAttribution.makeRequest({
        targetURL: result.payload.url,
        source: "urlbar",
        campaignID: Services.prefs.getStringPref(
          "browser.partnerlink.campaign.topsites"
        ),
      });
      if (!this.isPrivate && result.providerName === "UrlbarProviderTopSites") {
        // The position is 1-based for telemetry
        const position = selIndex + 1;
        Services.telemetry.keyedScalarAdd(
          SCALAR_CATEGORY_TOPSITES,
          `urlbar_${position}`,
          1
        );
        PartnerLinkAttribution.sendContextualServicesPing(
          {
            position,
            source: "urlbar",
            tile_id: result.payload.sponsoredTileId || -1,
            reporting_url: result.payload.sponsoredClickUrl,
            advertiser: result.payload.title.toLocaleLowerCase(),
          },
          CONTEXTUAL_SERVICES_PING_TYPES.TOPSITES_SELECTION
        );
      }
    }

    this._loadURL(
      url,
      event,
      where,
      openParams,
      {
        source: result.source,
        type: result.type,
      },
      browser
    );
  }

  /**
   * Called by the view when moving through results with the keyboard, and when
   * picking a result.  This sets the input value to the value of the result and
   * invalidates the pageproxystate.  It also sets the result that is associated
   * with the current input value.  If you need to set this result but don't
   * want to also set the input value, then use setResultForCurrentValue.
   *
   * @param {UrlbarResult} [result]
   *   The result that was selected or picked, null if no result was selected.
   * @param {Event} [event]
   *   The event that picked the result.
   * @param {string} [urlOverride]
   *   Normally the URL is taken from `result.payload.url`, but if `urlOverride`
   *   is specified, it's used instead.
   * @returns {boolean}
   *   Whether the value has been canonized
   */
  setValueFromResult({ result = null, event = null, urlOverride = null } = {}) {
    // Usually this is set by a previous input event, but in certain cases, like
    // when opening Top Sites on a loaded page, it wouldn't happen. To avoid
    // confusing the user, we always enforce it when a result changes our value.
    this.setPageProxyState("invalid", true);

    // A previous result may have previewed search mode. If we don't expect that
    // we might stay in a search mode of some kind, exit it now.
    if (
      this.searchMode?.isPreview &&
      !result?.payload.providesSearchMode &&
      !this.view.oneOffSearchButtons.selectedButton
    ) {
      this.searchMode = null;
    }

    if (!result) {
      // This happens when there's no selection, for example when moving to the
      // one-offs search settings button, or to the input field when Top Sites
      // are shown; then we must reset the input value.
      // Note that for Top Sites the last search string would be empty, thus we
      // must restore the last text value.
      // Note that unselected autofill results will still arrive in this
      // function with a non-null `result`. They are handled below.
      this.value = this._lastSearchString || this._valueOnLastSearch;
      this.setResultForCurrentValue(result);
      return false;
    }

    // The value setter clobbers the actiontype attribute, so we need this
    // helper to restore it afterwards.
    const setValueAndRestoreActionType = (value, allowTrim) => {
      this._setValue(value, allowTrim);

      switch (result.type) {
        case UrlbarUtils.RESULT_TYPE.TAB_SWITCH:
          this.setAttribute("actiontype", "switchtab");
          break;
        case UrlbarUtils.RESULT_TYPE.OMNIBOX:
          this.setAttribute("actiontype", "extension");
          break;
      }
    };

    // For autofilled results, the value that should be canonized is not the
    // autofilled value but the value that the user typed.
    let canonizedUrl = this._maybeCanonizeURL(
      event,
      result.autofill ? this._lastSearchString : this.value
    );
    if (canonizedUrl) {
      setValueAndRestoreActionType(canonizedUrl, true);
      this.setResultForCurrentValue(result);
      return true;
    }

    if (result.autofill) {
      let { value, selectionStart, selectionEnd } = result.autofill;
      this._autofillValue(value, selectionStart, selectionEnd);
    }

    if (result.payload.providesSearchMode) {
      let enteredSearchMode;
      // Only preview search mode if the result is selected.
      if (this.view.resultIsSelected(result)) {
        // Not starting a query means we will only preview search mode.
        enteredSearchMode = this.maybeConfirmSearchModeFromResult({
          result,
          checkValue: false,
          startQuery: false,
        });
      }
      if (!enteredSearchMode) {
        setValueAndRestoreActionType(this._getValueFromResult(result), true);
        this.searchMode = null;
      }
      this.setResultForCurrentValue(result);
      return false;
    }

    // If the url is trimmed but it's invalid (for example it has an unknown
    // single word host, or an unknown domain suffix), trimming
    // it would end up executing a search instead of visiting it.
    let allowTrim = true;
    if (
      (urlOverride || result.type == UrlbarUtils.RESULT_TYPE.URL) &&
      UrlbarPrefs.get("trimURLs")
    ) {
      let url = urlOverride || result.payload.url;
      if (url.startsWith(BrowserUIUtils.trimURLProtocol)) {
        let fixupInfo = this._getURIFixupInfo(BrowserUIUtils.trimURL(url));
        if (fixupInfo?.keywordAsSent) {
          allowTrim = false;
        }
      }
    }

    if (!result.autofill) {
      setValueAndRestoreActionType(
        this._getValueFromResult(result, urlOverride),
        allowTrim
      );
    }

    this.setResultForCurrentValue(result);
    return false;
  }

  /**
   * The input keeps track of the result associated with the current input
   * value.  This result can be set by calling either setValueFromResult or this
   * method.  Use this method when you need to set the result without also
   * setting the input value.  This can be the case when either the selection is
   * cleared and no other result becomes selected, or when the result is the
   * heuristic and we don't want to modify the value the user is typing.
   *
   * @param {UrlbarResult} result
   *   The result to associate with the current input value.
   */
  setResultForCurrentValue(result) {
    this._resultForCurrentValue = result;
  }

  /**
   * Called by the controller when the first result of a new search is received.
   * If it's an autofill result, then it may need to be autofilled, subject to a
   * few restrictions.
   *
   * @param {UrlbarResult} result
   *   The first result.
   */
  _autofillFirstResult(result) {
    if (!result.autofill) {
      return;
    }

    let isPlaceholderSelected =
      this.selectionEnd == this._autofillPlaceholder.length &&
      this.selectionStart == this._lastSearchString.length &&
      this._autofillPlaceholder
        .toLocaleLowerCase()
        .startsWith(this._lastSearchString.toLocaleLowerCase());

    // Don't autofill if there's already a selection (with one caveat described
    // next) or the cursor isn't at the end of the input.  But if there is a
    // selection and it's the autofill placeholder value, then do autofill.
    if (
      !isPlaceholderSelected &&
      !this._autofillIgnoresSelection &&
      (this.selectionStart != this.selectionEnd ||
        this.selectionEnd != this._lastSearchString.length)
    ) {
      return;
    }

    this.setValueFromResult({ result });
  }

  /**
   * Invoked by the controller when the first result is received.
   *
   * @param {UrlbarResult} firstResult
   *   The first result received.
   * @returns {boolean}
   *   True if this method canceled the query and started a new one.  False
   *   otherwise.
   */
  onFirstResult(firstResult) {
    // If the heuristic result has a keyword but isn't a keyword offer, we may
    // need to enter search mode.
    if (
      firstResult.heuristic &&
      firstResult.payload.keyword &&
      !firstResult.payload.providesSearchMode &&
      this.maybeConfirmSearchModeFromResult({
        result: firstResult,
        entry: "typed",
        checkValue: false,
      })
    ) {
      return true;
    }

    // To prevent selection flickering, we apply autofill on input through a
    // placeholder, without waiting for results. But, if the first result is
    // not an autofill one, the autofill prediction was wrong and we should
    // restore the original user typed string.
    if (firstResult.autofill) {
      this._autofillFirstResult(firstResult);
    } else if (
      this._autofillPlaceholder &&
      // Avoid clobbering added spaces (for token aliases, for example).
      !this.value.endsWith(" ")
    ) {
      this._setValue(this.window.gBrowser.userTypedValue, false);
    }

    return false;
  }

  /**
   * Starts a query based on the current input value.
   *
   * @param {boolean} [options.allowAutofill]
   *   Whether or not to allow providers to include autofill results.
   * @param {boolean} [options.autofillIgnoresSelection]
   *   Normally we autofill only if the cursor is at the end of the string,
   *   if this is set we'll autofill regardless of selection.
   * @param {string} [options.searchString]
   *   The search string.  If not given, the current input value is used.
   *   Otherwise, the current input value must start with this value.
   * @param {boolean} [options.resetSearchState]
   *   If this is the first search of a user interaction with the input, set
   *   this to true (the default) so that search-related state from the previous
   *   interaction doesn't interfere with the new interaction.  Otherwise set it
   *   to false so that state is maintained during a single interaction.  The
   *   intended use for this parameter is that it should be set to false when
   *   this method is called due to input events.
   * @param {event} [options.event]
   *   The user-generated event that triggered the query, if any.  If given, we
   *   will record engagement event telemetry for the query.
   */
  startQuery({
    allowAutofill = true,
    autofillIgnoresSelection = false,
    searchString = null,
    resetSearchState = true,
    event = null,
  } = {}) {
    if (!searchString) {
      searchString =
        this.getAttribute("pageproxystate") == "valid" ? "" : this.value;
    } else if (!this.value.startsWith(searchString)) {
      throw new Error("The current value doesn't start with the search string");
    }

    if (event) {
      this.controller.engagementEvent.start(event, searchString);
    }

    if (this._suppressStartQuery) {
      return;
    }

    this._autofillIgnoresSelection = autofillIgnoresSelection;
    if (resetSearchState) {
      this._resetSearchState();
    }

    this._lastSearchString = searchString;
    this._valueOnLastSearch = this.value;

    let options = {
      allowAutofill,
      isPrivate: this.isPrivate,
      maxResults: UrlbarPrefs.get("maxRichResults"),
      searchString,
      userContextId: this.window.gBrowser.selectedBrowser.getAttribute(
        "usercontextid"
      ),
      currentPage: this.window.gBrowser.currentURI.spec,
      formHistoryName: this.formHistoryName,
      allowSearchSuggestions:
        !event ||
        !UrlbarUtils.isPasteEvent(event) ||
        !event.data ||
        event.data.length <= UrlbarPrefs.get("maxCharsForSearchSuggestions"),
    };

    if (this.searchMode) {
      this.confirmSearchMode();
      options.searchMode = this.searchMode;
      if (this.searchMode.source) {
        options.sources = [this.searchMode.source];
      }
    }

    // TODO (Bug 1522902): This promise is necessary for tests, because some
    // tests are not listening for completion when starting a query through
    // other methods than startQuery (input events for example).
    this.lastQueryContextPromise = this.controller.startQuery(
      new UrlbarQueryContext(options)
    );
  }

  /**
   * Sets the input's value, starts a search, and opens the view.
   *
   * @param {string} value
   *   The input's value will be set to this value, and the search will
   *   use it as its query.
   * @param {UrlbarUtils.WEB_ENGINE_NAMES} [options.searchEngine]
   *   Search engine to use when the search is using a known alias.
   * @param {UrlbarUtils.SEARCH_MODE_ENTRY} [options.searchModeEntry]
   *   If provided, we will record this parameter as the search mode entry point
   *   in Telemetry. Consumers should provide this if they expect their call
   *   to enter search mode.
   * @param {boolean} [options.focus]
   *   If true, the urlbar will be focused.  If false, the focus will remain
   *   unchanged.
   */
  search(value, { searchEngine, searchModeEntry, focus = true } = {}) {
    if (focus) {
      this.focus();
    }

    let tokens = value.trim().split(UrlbarTokenizer.REGEXP_SPACES);
    // Enter search mode if the string starts with a restriction token.
    let searchMode = UrlbarUtils.searchModeForToken(tokens[0]);
    if (!searchMode && searchEngine) {
      searchMode = { engineName: searchEngine.name };
    }

    if (searchMode) {
      searchMode.entry = searchModeEntry;
      this.searchMode = searchMode;
      // Remove the restriction token/alias from the string to be searched for
      // in search mode.
      value = value.replace(tokens[0], "");
      if (UrlbarTokenizer.REGEXP_SPACES.test(value[0])) {
        // If there was a trailing space after the restriction token/alias,
        // remove it.
        value = value.slice(1);
      }
      this.inputField.value = value;
      this._revertOnBlurValue = this.value;
    } else if (Object.values(UrlbarTokenizer.RESTRICT).includes(tokens[0])) {
      this.searchMode = null;
      // If the entire value is a restricted token, append a space.
      if (Object.values(UrlbarTokenizer.RESTRICT).includes(value)) {
        value += " ";
      }
      this.inputField.value = value;
      this._revertOnBlurValue = this.value;
    } else {
      this.inputField.value = value;
    }

    // Avoid selecting the text if this method is called twice in a row.
    this.selectionStart = -1;

    // Note: proper IME Composition handling depends on the fact this generates
    // an input event, rather than directly invoking the controller; everything
    // goes through _on_input, that will properly skip the search until the
    // composition is committed. _on_input also skips the search when it's the
    // same as the previous search, but we want to allow consecutive searches
    // with the same string. So clear _lastSearchString first.
    this._lastSearchString = "";
    let event = this.document.createEvent("UIEvents");
    event.initUIEvent("input", true, false, this.window, 0);
    this.inputField.dispatchEvent(event);
  }

  /**
   * Focus without the focus styles.
   * This is used by Activity Stream and about:privatebrowsing for search hand-off.
   */
  setHiddenFocus() {
    this._hideFocus = true;
    if (this.focused) {
      this.removeAttribute("focused");
    } else {
      this.focus();
    }
  }

  /**
   * Restore focus styles.
   * This is used by Activity Stream and about:privatebrowsing for search hand-off.
   */
  removeHiddenFocus() {
    this._hideFocus = false;
    if (this.focused) {
      this.setAttribute("focused", "true");
      if (!UrlbarPrefs.get("browser.proton.urlbar.enabled")) {
        this.startLayoutExtend();
      }
    }
  }

  /**
   * Gets the search mode for a specific browser instance.
   *
   * @param {Browser} browser
   *   The search mode for this browser will be returned.
   * @param {boolean} [confirmedOnly]
   *   Normally, if the browser has both preview and confirmed modes, preview
   *   mode will be returned since it takes precedence.  If this argument is
   *   true, then only confirmed search mode will be returned, or null if
   *   search mode hasn't been confirmed.
   * @returns {object}
   *   A search mode object.  See setSearchMode documentation.  If the browser
   *   is not in search mode, then null is returned.
   */
  getSearchMode(browser, confirmedOnly = false) {
    let modes = this._searchModesByBrowser.get(browser);

    // Return copies so that callers don't modify the stored values.
    if (!confirmedOnly && modes?.preview) {
      return { ...modes.preview };
    }
    if (modes?.confirmed) {
      return { ...modes.confirmed };
    }
    return null;
  }

  /**
   * Sets search mode for a specific browser instance.  If the given browser is
   * selected, then this will also enter search mode.
   *
   * @param {object} searchMode
   *   A search mode object.
   * @param {string} searchMode.engineName
   *   The name of the search engine to restrict to.
   * @param {UrlbarUtils.RESULT_SOURCE} searchMode.source
   *   A result source to restrict to.
   * @param {string} searchMode.entry
   *   How search mode was entered. This is recorded in event telemetry. One of
   *   the values in UrlbarUtils.SEARCH_MODE_ENTRY.
   * @param {boolean} [searchMode.isPreview]
   *   If true, we will preview search mode. Search mode preview does not record
   *   telemetry and has slighly different UI behavior. The preview is exited in
   *   favor of full search mode when a query is executed. False should be
   *   passed if the caller needs to enter search mode but expects it will not
   *   be interacted with right away. Defaults to true.
   * @param {Browser} browser
   *   The browser for which to set search mode.
   */
  async setSearchMode(searchMode, browser) {
    let currentSearchMode = this.getSearchMode(browser);
    let areSearchModesSame =
      (!currentSearchMode && !searchMode) ||
      ObjectUtils.deepEqual(currentSearchMode, searchMode);

    // Exit search mode if the passed-in engine is invalid or hidden.
    if (searchMode?.engineName) {
      if (!Services.search.isInitialized) {
        await Services.search.init();
      }
      let engine = Services.search.getEngineByName(searchMode.engineName);
      if (!engine || engine.hidden) {
        searchMode = null;
      }
    }

    let { engineName, source, entry, isPreview = true } = searchMode || {};

    searchMode = null;

    if (engineName) {
      searchMode = { engineName };
      if (source) {
        searchMode.source = source;
      } else if (UrlbarUtils.WEB_ENGINE_NAMES.has(engineName)) {
        // History results for general-purpose search engines are often not
        // useful, so we hide them in search mode. See bug 1658646 for
        // discussion.
        searchMode.source = UrlbarUtils.RESULT_SOURCE.SEARCH;
      }
    } else if (source) {
      let sourceName = UrlbarUtils.getResultSourceName(source);
      if (sourceName) {
        searchMode = { source };
      } else {
        Cu.reportError(`Unrecognized source: ${source}`);
      }
    }

    if (searchMode) {
      searchMode.isPreview = isPreview;
      if (UrlbarUtils.SEARCH_MODE_ENTRY.has(entry)) {
        searchMode.entry = entry;
      } else {
        // If we see this value showing up in telemetry, we should review
        // search mode's entry points.
        searchMode.entry = "other";
      }

      // Add the search mode to the map.
      if (!searchMode.isPreview) {
        this._searchModesByBrowser.set(browser, {
          confirmed: searchMode,
        });
      } else {
        let modes = this._searchModesByBrowser.get(browser) || {};
        modes.preview = searchMode;
        this._searchModesByBrowser.set(browser, modes);
      }
    } else {
      this._searchModesByBrowser.delete(browser);
    }

    // Enter search mode if the browser is selected.
    if (browser == this.window.gBrowser.selectedBrowser) {
      this._updateSearchModeUI(searchMode);
      if (searchMode) {
        // Set userTypedValue to the query string so that it's properly restored
        // when switching back to the current tab and across sessions.
        this.window.gBrowser.userTypedValue = this.untrimmedValue;
        this.valueIsTyped = true;
        if (!searchMode.isPreview && !areSearchModesSame) {
          try {
            BrowserSearchTelemetry.recordSearchMode(searchMode);
          } catch (ex) {
            Cu.reportError(ex);
          }
        }
      }
    }
  }

  /**
   * Restores the current browser search mode from a previously stored state.
   */
  restoreSearchModeState() {
    let modes = this._searchModesByBrowser.get(
      this.window.gBrowser.selectedBrowser
    );
    this.searchMode = modes?.confirmed;
  }

  /**
   * Enters search mode with the default engine.
   */
  searchModeShortcut() {
    // We restrict to search results when entering search mode from this
    // shortcut to honor historical behaviour.
    this.searchMode = {
      source: UrlbarUtils.RESULT_SOURCE.SEARCH,
      engineName: UrlbarSearchUtils.getDefaultEngine(this.isPrivate).name,
      entry: "shortcut",
    };
    // The searchMode setter clears the input if pageproxystate is valid, so
    // we know at this point this.value will either be blank or the user's
    // typed string.
    this.search(this.value);
    this.select();
  }

  /**
   * Confirms the current search mode.
   */
  confirmSearchMode() {
    let searchMode = this.searchMode;
    if (searchMode?.isPreview) {
      searchMode.isPreview = false;
      this.searchMode = searchMode;

      // Unselect the one-off search button to ensure UI consistency.
      this.view.oneOffSearchButtons.selectedButton = null;
    }
  }

  // Getters and Setters below.

  get editor() {
    return this.inputField.editor;
  }

  get focused() {
    return this.document.activeElement == this.inputField;
  }

  get goButton() {
    return this.querySelector("#urlbar-go-button");
  }

  get value() {
    return this.inputField.value;
  }

  get untrimmedValue() {
    return this._untrimmedValue;
  }

  set value(val) {
    this._setValue(val, true);
  }

  get lastSearchString() {
    return this._lastSearchString;
  }

  get searchMode() {
    return this.getSearchMode(this.window.gBrowser.selectedBrowser);
  }

  set searchMode(searchMode) {
    this.setSearchMode(searchMode, this.window.gBrowser.selectedBrowser);
  }

  async updateLayoutBreakout() {
    if (!this._toolbar) {
      // Expanding requires a parent toolbar.
      return;
    }
    if (this.document.fullscreenElement) {
      // Toolbars are hidden in DOM fullscreen mode, so we can't get proper
      // layout information and need to retry after leaving that mode.
      this.window.addEventListener(
        "fullscreen",
        () => {
          this.updateLayoutBreakout();
        },
        { once: true }
      );
      return;
    }
    await this._updateLayoutBreakoutDimensions();
    if (!UrlbarPrefs.get("browser.proton.urlbar.enabled")) {
      this.startLayoutExtend();
    }
  }

  startLayoutExtend() {
    // Do not expand if:
    // The Urlbar does not support being expanded or it is already expanded
    if (
      !this.hasAttribute("breakout") ||
      this.hasAttribute("breakout-extend")
    ) {
      return;
    }
    if (UrlbarPrefs.get("browser.proton.urlbar.enabled") && !this.view.isOpen) {
      return;
    }
    // The Urlbar is unfocused or reduce motion is on and the view is closed.
    // gReduceMotion is accurate in most cases, but it is automatically set to
    // true when windows are loaded. We check `prefers-reduced-motion: reduce`
    // to ensure the user actually set prefers-reduced-motion. We check
    // gReduceMotion first to save work in the common case of having
    // prefers-reduced-motion disabled.
    if (
      !this.view.isOpen &&
      (this.getAttribute("focused") != "true" ||
        (this.window.gReduceMotion &&
          this.window.matchMedia("(prefers-reduced-motion: reduce)").matches))
    ) {
      return;
    }

    if (Cu.isInAutomation) {
      if (UrlbarPrefs.get("disableExtendForTests")) {
        this.setAttribute("breakout-extend-disabled", "true");
        return;
      }
      this.removeAttribute("breakout-extend-disabled");
    }

    this._toolbar.setAttribute("urlbar-exceeds-toolbar-bounds", "true");
    this.setAttribute("breakout-extend", "true");

    // Enable the animation only after the first extend call to ensure it
    // doesn't run when opening a new window.
    if (!this.hasAttribute("breakout-extend-animate")) {
      this.window.promiseDocumentFlushed(() => {
        this.window.requestAnimationFrame(() => {
          this.setAttribute("breakout-extend-animate", "true");
        });
      });
    }
  }

  endLayoutExtend() {
    // If reduce motion is enabled, we want to collapse the Urlbar here so the
    // user sees only sees two states: not expanded, and expanded with the view
    // open.
    if (!this.hasAttribute("breakout-extend") || this.view.isOpen) {
      return;
    }

    if (
      !UrlbarPrefs.get("browser.proton.urlbar.enabled") &&
      this.getAttribute("focused") == "true" &&
      (!this.window.gReduceMotion ||
        !this.window.matchMedia("(prefers-reduced-motion: reduce)").matches)
    ) {
      return;
    }

    this.removeAttribute("breakout-extend");
    this._toolbar.removeAttribute("urlbar-exceeds-toolbar-bounds");
  }

  /**
   * Updates the user interface to indicate whether the URI in the address bar
   * is different than the loaded page, because it's being edited or because a
   * search result is currently selected and is displayed in the location bar.
   *
   * @param {string} state
   *        The string "valid" indicates that the security indicators and other
   *        related user interface elments should be shown because the URI in
   *        the location bar matches the loaded page. The string "invalid"
   *        indicates that the URI in the location bar is different than the
   *        loaded page.
   * @param {boolean} [updatePopupNotifications]
   *        Indicates whether we should update the PopupNotifications
   *        visibility due to this change, otherwise avoid doing so as it is
   *        being handled somewhere else.
   */
  setPageProxyState(state, updatePopupNotifications) {
    let prevState = this.getAttribute("pageproxystate");

    this.setAttribute("pageproxystate", state);
    this._inputContainer.setAttribute("pageproxystate", state);
    this._identityBox.setAttribute("pageproxystate", state);

    if (state == "valid") {
      this._lastValidURLStr = this.value;
    }

    if (
      updatePopupNotifications &&
      prevState != state &&
      this.window.UpdatePopupNotificationsVisibility
    ) {
      this.window.UpdatePopupNotificationsVisibility();
    }
  }

  /**
   * When switching tabs quickly, TabSelect sometimes happens before
   * _adjustFocusAfterTabSwitch and due to the focus still being on the old
   * tab, we end up flickering the results pane briefly.
   */
  afterTabSwitchFocusChange() {
    this._gotFocusChange = true;
    this._afterTabSelectAndFocusChange();
  }

  /**
   * Confirms search mode and starts a new search if appropriate for the given
   * result.  See also _searchModeForResult.
   *
   * @param {string} entry
   *   The search mode entry point. See setSearchMode documentation for details.
   * @param {UrlbarResult} [result]
   *   The result to confirm. Defaults to the currently selected result.
   * @param {boolean} [checkValue]
   *   If true, the trimmed input value must equal the result's keyword in order
   *   to enter search mode.
   * @param {boolean} [startQuery]
   *   If true, start a query after entering search mode. Defaults to true.
   * @returns {boolean}
   *   True if we entered search mode and false if not.
   */
  maybeConfirmSearchModeFromResult({
    entry,
    result = this._resultForCurrentValue,
    checkValue = true,
    startQuery = true,
  }) {
    if (
      !result ||
      (checkValue && this.value.trim() != result.payload.keyword?.trim())
    ) {
      return false;
    }

    let searchMode = this._searchModeForResult(result, entry);
    if (!searchMode) {
      return false;
    }

    this.searchMode = searchMode;

    let value = result.payload.query?.trimStart() || "";
    this._setValue(value, false);

    if (startQuery) {
      this.startQuery({ allowAutofill: false });
    }

    return true;
  }

  observe(subject, topic, data) {
    switch (topic) {
      case SearchUtils.TOPIC_ENGINE_MODIFIED: {
        switch (data) {
          case SearchUtils.MODIFIED_TYPE.CHANGED:
          case SearchUtils.MODIFIED_TYPE.REMOVED: {
            let searchMode = this.searchMode;
            let engine = subject.QueryInterface(Ci.nsISearchEngine);
            if (searchMode?.engineName == engine.name) {
              // Exit search mode if the current search mode engine was removed.
              this.searchMode = searchMode;
            }
            break;
          }
        }
        break;
      }
    }
  }

  // Private methods below.

  _addObservers() {
    Services.obs.addObserver(this, SearchUtils.TOPIC_ENGINE_MODIFIED, true);
  }

  _getURIFixupInfo(searchString) {
    let flags =
      Ci.nsIURIFixup.FIXUP_FLAG_FIX_SCHEME_TYPOS |
      Ci.nsIURIFixup.FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP;
    if (this.isPrivate) {
      flags |= Ci.nsIURIFixup.FIXUP_FLAG_PRIVATE_CONTEXT;
    }
    try {
      return Services.uriFixup.getFixupURIInfo(searchString, flags);
    } catch (ex) {
      Cu.reportError(
        `An error occured while trying to fixup "${searchString}": ${ex}`
      );
    }
    return null;
  }

  _afterTabSelectAndFocusChange() {
    // We must have seen both events to proceed safely.
    if (!this._gotFocusChange || !this._gotTabSelect) {
      return;
    }
    this._gotFocusChange = this._gotTabSelect = false;

    this._resetSearchState();

    // Switching tabs doesn't always change urlbar focus, so we must try to
    // reopen here too, not just on focus.
    // We don't use the original TabSelect event because caching it causes
    // leaks on MacOS.
    if (this.view.autoOpen({ event: new CustomEvent("tabswitch") })) {
      return;
    }
    // The input may retain focus when switching tabs in which case we
    // need to close the view explicitly.
    this.view.close();
  }

  async _updateLayoutBreakoutDimensions() {
    // When this method gets called a second time before the first call
    // finishes, we need to disregard the first one.
    let updateKey = {};
    this._layoutBreakoutUpdateKey = updateKey;

    this.removeAttribute("breakout");
    this.textbox.parentNode.removeAttribute("breakout");

    await this.window.promiseDocumentFlushed(() => {});
    await new Promise(resolve => {
      this.window.requestAnimationFrame(() => {
        if (this._layoutBreakoutUpdateKey != updateKey) {
          return;
        }

        this.textbox.parentNode.style.setProperty(
          "--urlbar-container-height",
          px(getBoundsWithoutFlushing(this.textbox.parentNode).height)
        );
        this.textbox.style.setProperty(
          "--urlbar-height",
          px(getBoundsWithoutFlushing(this.textbox).height)
        );
        this.textbox.style.setProperty(
          "--urlbar-toolbar-height",
          px(getBoundsWithoutFlushing(this._toolbar).height)
        );

        this.setAttribute("breakout", "true");
        this.textbox.parentNode.setAttribute("breakout", "true");

        resolve();
      });
    });
  }

  _setValue(val, allowTrim) {
    // Don't expose internal about:reader URLs to the user.
    let originalUrl = ReaderMode.getOriginalUrlObjectForDisplay(val);
    if (originalUrl) {
      val = originalUrl.displaySpec;
    }
    this._untrimmedValue = val;

    if (allowTrim) {
      val = this._trimValue(val);
    }

    this.valueIsTyped = false;
    this._resultForCurrentValue = null;
    this.inputField.value = val;
    this.formatValue();
    this.removeAttribute("actiontype");

    // Dispatch ValueChange event for accessibility.
    let event = this.document.createEvent("Events");
    event.initEvent("ValueChange", true, true);
    this.inputField.dispatchEvent(event);

    return val;
  }

  _getValueFromResult(result, urlOverride = null) {
    switch (result.type) {
      case UrlbarUtils.RESULT_TYPE.KEYWORD:
        return result.payload.input;
      case UrlbarUtils.RESULT_TYPE.SEARCH: {
        let value = "";
        if (result.payload.keyword) {
          value += result.payload.keyword + " ";
        }
        value += result.payload.suggestion || result.payload.query;
        return value;
      }
      case UrlbarUtils.RESULT_TYPE.OMNIBOX:
        return result.payload.content;
    }

    try {
      let uri = Services.io.newURI(urlOverride || result.payload.url);
      if (uri) {
        return losslessDecodeURI(uri);
      }
    } catch (ex) {}

    return "";
  }

  /**
   * Resets some state so that searches from the user's previous interaction
   * with the input don't interfere with searches from a new interaction.
   */
  _resetSearchState() {
    this._lastSearchString = this.value;
    this._autofillPlaceholder = "";
  }

  /**
   * Autofills the autofill placeholder string if appropriate, and determines
   * whether autofill should be allowed for the new search started by an input
   * event.
   *
   * @param {string} value
   *   The new search string.
   * @returns {boolean}
   *   Whether autofill should be allowed in the new search.
   */
  _maybeAutofillOnInput(value) {
    // We allow autofill in local but not remote search modes.
    let allowAutofill =
      this.selectionEnd == value.length &&
      !this.searchMode?.engineName &&
      this.searchMode?.source != UrlbarUtils.RESULT_SOURCE.SEARCH;

    // Determine whether we can autofill the placeholder.  The placeholder is a
    // value that we autofill now, when the search starts and before we wait on
    // its first result, in order to prevent a flicker in the input caused by
    // the previous autofilled substring disappearing and reappearing when the
    // first result arrives.  Of course we can only autofill the placeholder if
    // it starts with the new search string, and we shouldn't autofill anything
    // if the caret isn't at the end of the input.
    if (
      !allowAutofill ||
      !UrlbarUtils.canAutofillURL(this._autofillPlaceholder, value)
    ) {
      this._autofillPlaceholder = "";
    } else if (
      this._autofillPlaceholder &&
      this.selectionEnd == this.value.length &&
      this._enableAutofillPlaceholder
    ) {
      let autofillValue =
        value + this._autofillPlaceholder.substring(value.length);
      this._autofillValue(autofillValue, value.length, autofillValue.length);
    }

    return allowAutofill;
  }

  _checkForRtlText(value) {
    let directionality = this.window.windowUtils.getDirectionFromText(value);
    if (directionality == this.window.windowUtils.DIRECTION_RTL) {
      return true;
    }
    return false;
  }

  /**
   * Invoked on overflow/underflow/scrollend events to update attributes
   * related to the input text directionality. Overflow fade masks use these
   * attributes to appear at the proper side of the urlbar.
   */
  _updateTextOverflow() {
    if (!this._overflowing) {
      this.removeAttribute("textoverflow");
      return;
    }

    let isRTL =
      this.getAttribute("domaindir") != "ltr" &&
      this._checkForRtlText(this.value);

    this.window.promiseDocumentFlushed(() => {
      // Check overflow again to ensure it didn't change in the meanwhile.
      let input = this.inputField;
      if (input && this._overflowing) {
        // Normally we would overflow at the final side of text direction,
        // though RTL domains may cause us to overflow at the opposite side.
        // This happens dynamically as a consequence of the input field contents
        // and the call to _ensureFormattedHostVisible, this code only reports
        // the final state of all that scrolling into an attribute, because
        // there's no other way to capture this in css.
        // Note it's also possible to scroll an unfocused input field using
        // SHIFT + mousewheel on Windows, or with just the mousewheel / touchpad
        // scroll (without modifiers) on Mac.
        let side = "both";
        if (isRTL) {
          if (input.scrollLeft == 0) {
            side = "left";
          } else if (input.scrollLeft == input.scrollLeftMin) {
            side = "right";
          }
        } else if (input.scrollLeft == 0) {
          side = "right";
        } else if (input.scrollLeft == input.scrollLeftMax) {
          side = "left";
        }

        this.window.requestAnimationFrame(() => {
          // And check once again, since we might have stopped overflowing
          // since the promiseDocumentFlushed callback fired.
          if (this._overflowing) {
            this.setAttribute("textoverflow", side);
          }
        });
      }
    });
  }

  _updateUrlTooltip() {
    if (this.focused || !this._overflowing) {
      this.inputField.removeAttribute("title");
    } else {
      this.inputField.setAttribute("title", this.untrimmedValue);
    }
  }

  _getSelectedValueForClipboard() {
    let selection = this.editor.selection;
    const flags =
      Ci.nsIDocumentEncoder.OutputPreformatted |
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
      let remainder = this.value.replace(selectedVal, "");
      if (remainder != "" && remainder[0] != "/") {
        return selectedVal;
      }
    }

    let uri;
    if (this.getAttribute("pageproxystate") == "valid") {
      uri = this.window.gBrowser.currentURI;
    } else {
      // The value could be:
      // 1. a trimmed url, set by selecting a result
      // 2. a search string set by selecting a result
      // 3. a url that was confirmed but didn't finish loading yet
      // If it's an url the untrimmedValue should resolve to a valid URI,
      // otherwise it's a search string that should be copied as-is.
      try {
        uri = Services.io.newURI(this._untrimmedValue);
      } catch (ex) {
        return selectedVal;
      }
    }
    uri = this.makeURIReadable(uri);
    let displaySpec = uri.displaySpec;

    // If the entire URL is selected, just use the actual loaded URI,
    // unless we want a decoded URI, or it's a data: or javascript: URI,
    // since those are hard to read when encoded.
    if (
      this.value == selectedVal &&
      !uri.schemeIs("javascript") &&
      !uri.schemeIs("data") &&
      !UrlbarPrefs.get("decodeURLsOnCopy")
    ) {
      return displaySpec;
    }

    // Just the beginning of the URL is selected, or we want a decoded
    // url. First check for a trimmed value.

    if (
      !selectedVal.startsWith(BrowserUIUtils.trimURLProtocol) &&
      // Note _trimValue may also trim a trailing slash, thus we can't just do
      // a straight string compare to tell if the protocol was trimmed.
      !displaySpec.startsWith(this._trimValue(displaySpec))
    ) {
      selectedVal = BrowserUIUtils.trimURLProtocol + selectedVal;
    }

    return selectedVal;
  }

  _toggleActionOverride(event) {
    // Ignore repeated KeyboardEvents.
    if (event.repeat) {
      return;
    }
    if (
      event.keyCode == KeyEvent.DOM_VK_SHIFT ||
      event.keyCode == KeyEvent.DOM_VK_ALT ||
      event.keyCode ==
        (AppConstants.platform == "macosx"
          ? KeyEvent.DOM_VK_META
          : KeyEvent.DOM_VK_CONTROL)
    ) {
      if (event.type == "keydown") {
        this._actionOverrideKeyCount++;
        this.setAttribute("actionoverride", "true");
        this.view.panel.setAttribute("actionoverride", "true");
      } else if (
        this._actionOverrideKeyCount &&
        --this._actionOverrideKeyCount == 0
      ) {
        this._clearActionOverride();
      }
    }
  }

  _clearActionOverride() {
    this._actionOverrideKeyCount = 0;
    this.removeAttribute("actionoverride");
    this.view.panel.removeAttribute("actionoverride");
  }

  /**
   * Get the url to load for the search query and records in telemetry that it
   * is being loaded.
   *
   * @param {nsISearchEngine} engine
   *   The engine to generate the query for.
   * @param {Event} event
   *   The event that triggered this query.
   * @param {object} searchActionDetails
   *   The details associated with this search query.
   * @param {boolean} searchActionDetails.isSuggestion
   *   True if this query was initiated from a suggestion from the search engine.
   * @param {boolean} searchActionDetails.alias
   *   True if this query was initiated via a search alias.
   * @param {boolean} searchActionDetails.isFormHistory
   *   True if this query was initiated from a form history result.
   * @param {string} searchActionDetails.url
   *   The url this query was triggered with.
   */
  _recordSearch(engine, event, searchActionDetails = {}) {
    const isOneOff = this.view.oneOffSearchButtons.eventTargetIsAOneOff(event);

    BrowserSearchTelemetry.recordSearch(
      this.window.gBrowser.selectedBrowser,
      engine,
      // Without checking !isOneOff, we might record the string
      // oneoff_urlbar-searchmode in the SEARCH_COUNTS probe (in addition to
      // oneoff_urlbar and oneoff_searchbar). The extra information is not
      // necessary; the intent is the same regardless of whether the user is
      // in search mode when they do a key-modified click/enter on a one-off.
      this.searchMode && !isOneOff ? "urlbar-searchmode" : "urlbar",
      { ...searchActionDetails, isOneOff }
    );
  }

  /**
   * Shortens the given value, usually by removing http:// and trailing slashes.
   *
   * @param {string} val
   *   The string to be trimmed if it appears to be URI
   * @returns {string}
   *   The trimmed string
   */
  _trimValue(val) {
    return UrlbarPrefs.get("trimURLs") ? BrowserUIUtils.trimURL(val) : val;
  }

  /**
   * If appropriate, this prefixes a search string with 'www.' and suffixes it
   * with browser.fixup.alternate.suffix prior to navigating.
   *
   * @param {Event} event
   *   The event that triggered this query.
   * @param {string} value
   *   The search string that should be canonized.
   * @returns {string}
   *   Returns the canonized URL if available and null otherwise.
   */
  _maybeCanonizeURL(event, value) {
    // Only add the suffix when the URL bar value isn't already "URL-like",
    // and only if we get a keyboard event, to match user expectations.
    if (
      !(event instanceof KeyboardEvent) ||
      event._disableCanonization ||
      !event.ctrlKey ||
      !UrlbarPrefs.get("ctrlCanonizesURLs") ||
      !/^\s*[^.:\/\s]+(?:\/.*|\s*)$/i.test(value)
    ) {
      return null;
    }

    let suffix = Services.prefs.getCharPref("browser.fixup.alternate.suffix");
    if (!suffix.endsWith("/")) {
      suffix += "/";
    }

    // trim leading/trailing spaces (bug 233205)
    value = value.trim();

    // Tack www. and suffix on.  If user has appended directories, insert
    // suffix before them (bug 279035).  Be careful not to get two slashes.
    let firstSlash = value.indexOf("/");
    if (firstSlash >= 0) {
      value =
        value.substring(0, firstSlash) +
        suffix +
        value.substring(firstSlash + 1);
    } else {
      value = value + suffix;
    }

    try {
      const info = Services.uriFixup.getFixupURIInfo(
        value,
        Ci.nsIURIFixup.FIXUP_FLAGS_MAKE_ALTERNATE_URI
      );
      value = info.fixedURI.spec;
    } catch (ex) {
      Cu.reportError(
        `An error occured while trying to fixup "${value}": ${ex}`
      );
    }

    this.value = value;
    return value;
  }

  /**
   * Autofills a value into the input.  The value will be autofilled regardless
   * of the input's current value.
   *
   * @param {string} value
   *   The value to autofill.
   * @param {integer} selectionStart
   *   The new selectionStart.
   * @param {integer} selectionEnd
   *   The new selectionEnd.
   */
  _autofillValue(value, selectionStart, selectionEnd) {
    // The autofilled value may be a URL that includes a scheme at the
    // beginning.  Do not allow it to be trimmed.
    this._setValue(value, false);
    this.selectionStart = selectionStart;
    this.selectionEnd = selectionEnd;
    this._autofillPlaceholder = value;
  }

  /**
   * Loads the url in the appropriate place.
   *
   * @param {string} url
   *   The URL to open.
   * @param {Event} event
   *   The event that triggered to load the url.
   * @param {string} openUILinkWhere
   *   Where we expect the result to be opened.
   * @param {object} params
   *   The parameters related to how and where the result will be opened.
   *   Further supported paramters are listed in utilityOverlay.js#openUILinkIn.
   * @param {object} params.triggeringPrincipal
   *   The principal that the action was triggered from.
   * @param {nsIInputStream} [params.postData]
   *   The POST data associated with a search submission.
   * @param {boolean} [params.allowInheritPrincipal]
   *   Whether the principal can be inherited.
   * @param {object} [resultDetails]
   *   Details of the selected result, if any.
   * @param {UrlbarUtils.RESULT_TYPE} [result.type]
   *   Details of the result type, if any.
   * @param {UrlbarUtils.RESULT_SOURCE} [result.source]
   *   Details of the result source, if any.
   * @param {object} browser [optional] the browser to use for the load.
   */
  _loadURL(
    url,
    event,
    openUILinkWhere,
    params,
    resultDetails = null,
    browser = this.window.gBrowser.selectedBrowser
  ) {
    // No point in setting these because we'll handleRevert() a few rows below.
    if (openUILinkWhere == "current") {
      this.value = url;
      browser.userTypedValue = url;
    }

    // No point in setting this if we are loading in a new window.
    if (
      openUILinkWhere != "window" &&
      this.window.gInitialPages.includes(url)
    ) {
      browser.initialPageLoadedFromUserAction = url;
    }

    try {
      UrlbarUtils.addToUrlbarHistory(url, this.window);
    } catch (ex) {
      // Things may go wrong when adding url to session history,
      // but don't let that interfere with the loading of the url.
      Cu.reportError(ex);
    }

    // TODO: When bug 1498553 is resolved, we should be able to
    // remove the !triggeringPrincipal condition here.
    if (
      !params.triggeringPrincipal ||
      params.triggeringPrincipal.isSystemPrincipal
    ) {
      // Reset DOS mitigations for the basic auth prompt.
      delete browser.authPromptAbuseCounter;

      // Reset temporary permissions on the current tab if the user reloads
      // the tab via the urlbar.
      if (
        openUILinkWhere == "current" &&
        browser.currentURI &&
        url === browser.currentURI.spec
      ) {
        this.window.SitePermissions.clearTemporaryBlockPermissions(browser);
      }
    }

    params.allowThirdPartyFixup = true;

    if (openUILinkWhere == "current") {
      params.targetBrowser = browser;
      params.indicateErrorPageLoad = true;
      params.allowPinnedTabHostChange = true;
      params.allowPopups = url.startsWith("javascript:");
    } else {
      params.initiatingDoc = this.window.document;
    }

    if (event?.keyCode === KeyEvent.DOM_VK_RETURN) {
      if (openUILinkWhere === "current") {
        params.avoidBrowserFocus = true;
        this._keyDownEnterDeferred?.resolve(browser);
      }
    }

    // Focus the content area before triggering loads, since if the load
    // occurs in a new tab, we want focus to be restored to the content
    // area when the current tab is re-selected.
    if (!params.avoidBrowserFocus) {
      browser.focus();
      // Make sure the domain name stays visible for spoof protection and usability.
      this.selectionStart = this.selectionEnd = 0;
    }

    if (openUILinkWhere != "current") {
      this.handleRevert();
    }

    // Notify about the start of navigation.
    this._notifyStartNavigation(resultDetails);

    try {
      this.window.openTrustedLinkIn(url, openUILinkWhere, params);
    } catch (ex) {
      // This load can throw an exception in certain cases, which means
      // we'll want to replace the URL with the loaded URL:
      if (ex.result != Cr.NS_ERROR_LOAD_SHOWED_ERRORPAGE) {
        this.handleRevert();
      }
    }

    // If we show the focus border after closing the view, it would appear to
    // flash since this._on_blur would remove it immediately after.
    this.view.close({ showFocusBorder: false });
  }

  /**
   * Determines where a URL/page should be opened.
   *
   * @param {Event} event the event triggering the opening.
   * @returns {"current" | "tabshifted" | "tab" | "save" | "window"}
   */
  _whereToOpen(event) {
    let isKeyboardEvent = event instanceof KeyboardEvent;
    let reuseEmpty = isKeyboardEvent;
    let where = undefined;
    if (
      isKeyboardEvent &&
      (event.altKey || event.getModifierState("AltGraph"))
    ) {
      // We support using 'alt' to open in a tab, because ctrl/shift
      // might be used for canonizing URLs:
      where = event.shiftKey ? "tabshifted" : "tab";
    } else if (
      isKeyboardEvent &&
      event.ctrlKey &&
      UrlbarPrefs.get("ctrlCanonizesURLs")
    ) {
      // If we're allowing canonization, and this is a key event with ctrl
      // pressed, open in current tab to allow ctrl-enter to canonize URL.
      where = "current";
    } else {
      where = this.window.whereToOpenLink(event, false, false);
    }
    if (UrlbarPrefs.get("openintab")) {
      if (where == "current") {
        where = "tab";
      } else if (where == "tab") {
        where = "current";
      }
      reuseEmpty = true;
    }
    if (
      where == "tab" &&
      reuseEmpty &&
      this.window.gBrowser.selectedTab.isEmpty
    ) {
      where = "current";
    }
    return where;
  }

  _initCopyCutController() {
    this._copyCutController = new CopyCutController(this);
    this.inputField.controllers.insertControllerAt(0, this._copyCutController);
  }

  _initPasteAndGo() {
    let inputBox = this.querySelector("moz-input-box");
    let contextMenu = inputBox.menupopup;
    let insertLocation = contextMenu.firstElementChild;
    while (
      insertLocation.nextElementSibling &&
      insertLocation.getAttribute("cmd") != "cmd_paste"
    ) {
      insertLocation = insertLocation.nextElementSibling;
    }
    if (!insertLocation) {
      return;
    }

    let pasteAndGo = this.document.createXULElement("menuitem");
    pasteAndGo.id = "paste-and-go";
    let label = Services.strings
      .createBundle("chrome://browser/locale/browser.properties")
      .GetStringFromName("pasteAndGo.label");
    pasteAndGo.setAttribute("label", label);
    pasteAndGo.setAttribute("anonid", "paste-and-go");
    pasteAndGo.addEventListener("command", () => {
      this._suppressStartQuery = true;

      this.select();
      this.window.goDoCommand("cmd_paste");
      this.setResultForCurrentValue(null);
      this.handleCommand();

      this._suppressStartQuery = false;
    });

    contextMenu.addEventListener("popupshowing", () => {
      // Close the results pane when the input field contextual menu is open,
      // because paste and go doesn't want a result selection.
      this.view.close();

      let controller = this.document.commandDispatcher.getControllerForCommand(
        "cmd_paste"
      );
      let enabled = controller.isCommandEnabled("cmd_paste");
      if (enabled) {
        pasteAndGo.removeAttribute("disabled");
      } else {
        pasteAndGo.setAttribute("disabled", "true");
      }
    });

    insertLocation.insertAdjacentElement("afterend", pasteAndGo);
  }

  /**
   * This notifies observers that the user has entered or selected something in
   * the URL bar which will cause navigation.
   *
   * We use the observer service, so that we don't need to load extra facilities
   * if they aren't being used, e.g. WebNavigation.
   *
   * @param {UrlbarResult} result
   *   Details of the result that was selected, if any.
   */
  _notifyStartNavigation(result) {
    Services.obs.notifyObservers({ result }, "urlbar-user-start-navigation");
  }

  /**
   * Returns a search mode object if a result should enter search mode when
   * selected.
   *
   * @param {UrlbarResult} result
   * @param {string} [entry]
   *   If provided, this will be recorded as the entry point into search mode.
   *   See setSearchMode() documentation for details.
   * @returns {object} A search mode object. Null if search mode should not be
   *   entered. See setSearchMode documentation for details.
   */
  _searchModeForResult(result, entry = null) {
    // Search mode is determined by the result's keyword or engine.
    if (!result.payload.keyword && !result.payload.engine) {
      return null;
    }

    let searchMode = UrlbarUtils.searchModeForToken(result.payload.keyword);
    // If result.originalEngine is set, then the user is Alt+Tabbing
    // through the one-offs, so the keyword doesn't match the engine.
    if (
      !searchMode &&
      result.payload.engine &&
      (!result.payload.originalEngine ||
        result.payload.engine == result.payload.originalEngine)
    ) {
      searchMode = { engineName: result.payload.engine };
    }

    if (searchMode) {
      if (entry) {
        searchMode.entry = entry;
      } else {
        switch (result.providerName) {
          case "UrlbarProviderTopSites":
            searchMode.entry = "topsites_urlbar";
            break;
          case "TabToSearch":
            if (result.payload.dynamicType) {
              searchMode.entry = "tabtosearch_onboard";
            } else {
              searchMode.entry = "tabtosearch";
            }
            break;
          default:
            searchMode.entry = "keywordoffer";
            break;
        }
      }
    }

    return searchMode;
  }

  /**
   * Updates the UI so that search mode is either entered or exited.
   *
   * @param {object} searchMode
   *   See setSearchMode documentation.  If null, then search mode is exited.
   */
  _updateSearchModeUI(searchMode) {
    let { engineName, source } = searchMode || {};

    // As an optimization, bail if the given search mode is null but search mode
    // is already inactive.  Otherwise browser_preferences_usage.js fails due to
    // accessing the browser.urlbar.placeholderName pref (via the call to
    // BrowserSearch.initPlaceHolder below) too many times.  That test does not
    // enter search mode, but it triggers many calls to this method with a null
    // search mode, via setURI.
    if (!engineName && !source && !this.hasAttribute("searchmode")) {
      return;
    }

    this._searchModeIndicatorTitle.textContent = "";
    this._searchModeLabel.textContent = "";
    this._searchModeIndicatorTitle.removeAttribute("data-l10n-id");
    this._searchModeLabel.removeAttribute("data-l10n-id");

    if (!engineName && !source) {
      try {
        // This will throw before DOMContentLoaded in
        // PrivateBrowsingUtils.privacyContextFromWindow because
        // aWindow.docShell is null.
        this.window.BrowserSearch.initPlaceHolder(true);
      } catch (ex) {}
      this.removeAttribute("searchmode");
      return;
    }

    if (engineName) {
      // Set text content for the search mode indicator.
      this._searchModeIndicatorTitle.textContent = engineName;
      this._searchModeLabel.textContent = engineName;
      this.document.l10n.setAttributes(
        this.inputField,
        UrlbarUtils.WEB_ENGINE_NAMES.has(engineName)
          ? "urlbar-placeholder-search-mode-web-2"
          : "urlbar-placeholder-search-mode-other-engine",
        { name: engineName }
      );
    } else if (source) {
      let sourceName = UrlbarUtils.getResultSourceName(source);
      let l10nID = `urlbar-search-mode-${sourceName}`;
      this.document.l10n.setAttributes(this._searchModeIndicatorTitle, l10nID);
      this.document.l10n.setAttributes(this._searchModeLabel, l10nID);
      this.document.l10n.setAttributes(
        this.inputField,
        `urlbar-placeholder-search-mode-other-${sourceName}`
      );
    }

    this.toggleAttribute("searchmode", true);
    // Clear autofill.
    if (this._autofillPlaceholder && this.window.gBrowser.userTypedValue) {
      this.value = this.window.gBrowser.userTypedValue;
    }
    // Search mode should only be active when pageproxystate is invalid.
    if (this.getAttribute("pageproxystate") == "valid") {
      this.value = "";
      this.setPageProxyState("invalid", true);
    }
  }

  /**
   * Determines if we should select all the text in the Urlbar based on the
   *  Urlbar state, and whether the selection is empty.
   */
  _maybeSelectAll() {
    if (
      !this._preventClickSelectsAll &&
      this._compositionState != UrlbarUtils.COMPOSITION.COMPOSING &&
      this.document.activeElement == this.inputField &&
      this.inputField.selectionStart == this.inputField.selectionEnd
    ) {
      this.select();
    }
  }

  // Event handlers below.

  _on_command(event) {
    // Something is executing a command, likely causing a focus change. This
    // should not be recorded as an abandonment. If the user is entering search
    // mode from a one-off, then they are in the same engagement and we should
    // not discard.
    if (
      !event.target.classList.contains("searchbar-engine-one-off-item") ||
      this.searchMode?.entry != "oneoff"
    ) {
      this.controller.engagementEvent.discard();
    }
  }

  _on_blur(event) {
    this.focusedViaMousedown = false;
    // We cannot count every blur events after a missed engagement as abandoment
    // because the user may have clicked on some view element that executes
    // a command causing a focus change. For example opening preferences from
    // the oneoff settings button.
    // For now we detect that case by discarding the event on command, but we
    // may want to figure out a more robust way to detect abandonment.
    this.controller.engagementEvent.record(event, {
      searchString: this._lastSearchString,
    });

    this.removeAttribute("focused");
    if (!UrlbarPrefs.get("browser.proton.urlbar.enabled")) {
      this.endLayoutExtend();
    }

    if (this._autofillPlaceholder && this.window.gBrowser.userTypedValue) {
      // If we were autofilling, remove the autofilled portion, by restoring
      // the value to the last typed one.
      this.value = this.window.gBrowser.userTypedValue;
    } else if (this.value == this._focusUntrimmedValue) {
      // If the value was untrimmed by _on_focus and didn't change, trim it.
      this.value = this._focusUntrimmedValue;
    }
    this._focusUntrimmedValue = null;

    this.formatValue();
    this._resetSearchState();

    // In certain cases, like holding an override key and confirming an entry,
    // we don't key a keyup event for the override key, thus we make this
    // additional cleanup on blur.
    this._clearActionOverride();

    // The extension input sessions depends more on blur than on the fact we
    // actually cancel a running query, so we do it here.
    if (ExtensionSearchHandler.hasActiveInputSession()) {
      ExtensionSearchHandler.handleInputCancelled();
    }

    // Respect the autohide preference for easier inspecting/debugging via
    // the browser toolbox.
    if (!UrlbarPrefs.get("ui.popup.disable_autohide")) {
      this.view.close();
    }

    if (this._revertOnBlurValue == this.value) {
      this.handleRevert();
    }
    this._revertOnBlurValue = null;

    // We may have hidden popup notifications, show them again if necessary.
    if (
      this.getAttribute("pageproxystate") != "valid" &&
      this.window.UpdatePopupNotificationsVisibility
    ) {
      this.window.UpdatePopupNotificationsVisibility();
    }

    // If user move the focus to another component while pressing Enter key,
    // then keyup at that component, as we can't get the event, clear the promise.
    if (this._keyDownEnterDeferred) {
      this._keyDownEnterDeferred.resolve();
      this._keyDownEnterDeferred = null;
    }
    this._isKeyDownWithCtrl = false;

    Services.obs.notifyObservers(null, "urlbar-blur");
  }

  _on_click(event) {
    if (
      event.target == this.inputField ||
      event.target == this._inputContainer ||
      event.target.id == SEARCH_BUTTON_ID
    ) {
      this._maybeSelectAll();
    }

    if (event.target == this._searchModeIndicatorClose && event.button != 2) {
      this.searchMode = null;
      this.view.oneOffSearchButtons.selectedButton = null;
      if (this.view.isOpen) {
        this.startQuery({
          event,
        });
      }
    }
  }

  _on_contextmenu(event) {
    // Context menu opened via keyboard shortcut.
    if (!event.button) {
      return;
    }

    this._maybeSelectAll();
  }

  _on_focus(event) {
    if (!this._hideFocus) {
      this.setAttribute("focused", "true");
    }

    // If the value was trimmed, check whether we should untrim it.
    // This is necessary when a protocol was typed, but the whole url has
    // invalid parts, like the origin, then editing and confirming the trimmed
    // value would execute a search instead of visiting the typed url.
    if (this.value != this._untrimmedValue) {
      let untrim = false;
      let fixedURI = this._getURIFixupInfo(this.value)?.preferredURI;
      if (fixedURI) {
        try {
          let expectedURI = Services.io.newURI(this._untrimmedValue);
          untrim = fixedURI.displaySpec != expectedURI.displaySpec;
        } catch (ex) {
          untrim = true;
        }
      }
      if (untrim) {
        this.inputField.value = this._focusUntrimmedValue = this._untrimmedValue;
      }
    }

    if (!UrlbarPrefs.get("browser.proton.urlbar.enabled")) {
      this.startLayoutExtend();
    }

    if (this.focusedViaMousedown) {
      this.view.autoOpen({ event });
    } else if (this.inputField.hasAttribute("refocused-by-panel")) {
      this._maybeSelectAll();
    }

    this._updateUrlTooltip();
    this.formatValue();

    // Hide popup notifications, to reduce visual noise.
    if (
      this.getAttribute("pageproxystate") != "valid" &&
      this.window.UpdatePopupNotificationsVisibility
    ) {
      this.window.UpdatePopupNotificationsVisibility();
    }

    Services.obs.notifyObservers(null, "urlbar-focus");
  }

  _on_mouseover(event) {
    this._updateUrlTooltip();
  }

  _on_draggableregionleftmousedown(event) {
    if (!UrlbarPrefs.get("ui.popup.disable_autohide")) {
      this.view.close();
    }
  }

  _on_mousedown(event) {
    switch (event.currentTarget) {
      case this.textbox:
        this._mousedownOnUrlbarDescendant = true;

        if (
          event.target != this.inputField &&
          event.target != this._inputContainer &&
          event.target.id != SEARCH_BUTTON_ID
        ) {
          break;
        }

        this.focusedViaMousedown = !this.focused;
        this._preventClickSelectsAll = this.focused;

        if (event.target != this.inputField) {
          this.focus();
        }

        // The rest of this case only cares about left clicks.
        if (event.button != 0) {
          break;
        }

        // Clear any previous selection unless we are focused, to ensure it
        // doesn't affect drag selection.
        if (this.focusedViaMousedown) {
          this.selectionStart = this.selectionEnd = 0;
        }

        if (event.target.id == SEARCH_BUTTON_ID) {
          this._preventClickSelectsAll = true;
          this.search(UrlbarTokenizer.RESTRICT.SEARCH);
        } else {
          // Do not suppress the focus border if we are already focused. If we
          // did, we'd hide the focus border briefly then show it again if the
          // user has Top Sites disabled, creating a flashing effect.
          this.view.autoOpen({
            event,
            suppressFocusBorder: !this.hasAttribute("focused"),
          });
        }
        break;
      case this.window:
        if (this._mousedownOnUrlbarDescendant) {
          this._mousedownOnUrlbarDescendant = false;
          break;
        }
        // Don't close the view when clicking on a tab; we may want to keep the
        // view open on tab switch, and the TabSelect event arrived earlier.
        if (event.target.closest("tab")) {
          break;
        }
        // Close the view when clicking on toolbars and other UI pieces that
        // might not automatically remove focus from the input.
        // Respect the autohide preference for easier inspecting/debugging via
        // the browser toolbox.
        if (!UrlbarPrefs.get("ui.popup.disable_autohide")) {
          this.view.close();
        }
        break;
    }
  }

  _on_input(event) {
    let value = this.value;
    this.valueIsTyped = true;
    this._untrimmedValue = value;
    this._resultForCurrentValue = null;

    this.window.gBrowser.userTypedValue = value;
    // Unset userSelectionBehavior because the user is modifying the search
    // string, thus there's no valid selection. This is also used by the view
    // to set "aria-activedescendant", thus it should never get stale.
    this.controller.userSelectionBehavior = "none";

    let compositionState = this._compositionState;
    let compositionClosedPopup = this._compositionClosedPopup;

    // Clear composition values if we're no more composing.
    if (this._compositionState != UrlbarUtils.COMPOSITION.COMPOSING) {
      this._compositionState = UrlbarUtils.COMPOSITION.NONE;
      this._compositionClosedPopup = false;
    }

    if (value) {
      this.setAttribute("usertyping", "true");
    } else {
      this.removeAttribute("usertyping");
    }
    this.removeAttribute("actiontype");

    if (
      this.getAttribute("pageproxystate") == "valid" &&
      this.value != this._lastValidURLStr
    ) {
      this.setPageProxyState("invalid", true);
    }

    if (!this.view.isOpen) {
      this.view.clear();
    } else if (!value && !UrlbarPrefs.get("suggest.topsites")) {
      this.view.clear();
      if (!this.searchMode || !this.view.oneOffSearchButtons.hasView) {
        this.view.close();
        return;
      }
    }

    this.view.removeAccessibleFocus();

    // During composition with an IME, the following events happen in order:
    // 1. a compositionstart event
    // 2. some input events
    // 3. a compositionend event
    // 4. an input event

    // We should do nothing during composition or if composition was canceled
    // and we didn't close the popup on composition start.
    if (
      !UrlbarPrefs.get("keepPanelOpenDuringImeComposition") &&
      (compositionState == UrlbarUtils.COMPOSITION.COMPOSING ||
        (compositionState == UrlbarUtils.COMPOSITION.CANCELED &&
          !compositionClosedPopup))
    ) {
      return;
    }

    // Autofill only when text is inserted (i.e., event.data is not empty) and
    // it's not due to pasting.
    const allowAutofill =
      (!UrlbarPrefs.get("keepPanelOpenDuringImeComposition") ||
        compositionState !== UrlbarUtils.COMPOSITION.COMPOSING) &&
      !!event.data &&
      !UrlbarUtils.isPasteEvent(event) &&
      this._maybeAutofillOnInput(value);

    this.startQuery({
      searchString: value,
      allowAutofill,
      resetSearchState: false,
      event,
    });
  }

  _on_select(event) {
    // On certain user input, AutoCopyListener::OnSelectionChange() updates
    // the primary selection with user-selected text (when supported).
    // Selection::NotifySelectionListeners() then dispatches a "select" event
    // under similar conditions via TextInputListener::OnSelectionChange().
    // This event is received here in order to replace the primary selection
    // from the editor with text having the adjustments of
    // _getSelectedValueForClipboard(), such as adding the scheme for the url.
    //
    // Other "select" events are also received, however, and must be excluded.
    if (
      // _suppressPrimaryAdjustment is set during select().  Don't update
      // the primary selection because that is not the intent of user input,
      // which may be new tab or urlbar focus.
      this._suppressPrimaryAdjustment ||
      // The check on isHandlingUserInput filters out async "select" events
      // from setSelectionRange(), which occur when autofill text is selected.
      !this.window.windowUtils.isHandlingUserInput ||
      !Services.clipboard.supportsSelectionClipboard()
    ) {
      return;
    }

    let val = this._getSelectedValueForClipboard();
    if (!val) {
      return;
    }

    ClipboardHelper.copyStringToClipboard(
      val,
      Services.clipboard.kSelectionClipboard
    );
  }

  _on_overflow(event) {
    const targetIsPlaceholder =
      event.originalTarget.implementedPseudoElement == "::placeholder";
    // We only care about the non-placeholder text.
    // This shouldn't be needed, see bug 1487036.
    if (targetIsPlaceholder) {
      return;
    }
    this._overflowing = true;
    this._updateTextOverflow();
  }

  _on_underflow(event) {
    const targetIsPlaceholder =
      event.originalTarget.implementedPseudoElement == "::placeholder";
    // We only care about the non-placeholder text.
    // This shouldn't be needed, see bug 1487036.
    if (targetIsPlaceholder) {
      return;
    }
    this._overflowing = false;

    this._updateTextOverflow();

    this._updateUrlTooltip();
  }

  _on_paste(event) {
    let originalPasteData = event.clipboardData.getData("text/plain");
    if (!originalPasteData) {
      return;
    }

    let oldValue = this.inputField.value;
    let oldStart = oldValue.substring(0, this.selectionStart);
    // If there is already non-whitespace content in the URL bar
    // preceding the pasted content, it's not necessary to check
    // protocols used by the pasted content:
    if (oldStart.trim()) {
      return;
    }
    let oldEnd = oldValue.substring(this.selectionEnd);

    let isURLAssumed = true;
    try {
      const { keywordAsSent } = Services.uriFixup.getFixupURIInfo(
        originalPasteData,
        Ci.nsIURIFixup.FIXUP_FLAG_FIX_SCHEME_TYPOS |
          Ci.nsIURIFixup.FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP
      );
      isURLAssumed = !keywordAsSent;
    } catch (e) {}

    // In some cases, the data pasted will contain newline codes. In order to
    // achive the behavior expected by user, remove newline codes when URL is
    // assumed. When keywords are assumed, replace all whitespace characters
    // including newline with a space.
    let pasteData = isURLAssumed
      ? originalPasteData.replace(/[\r\n]/g, "")
      : originalPasteData.replace(/\s/g, " ");

    pasteData = UrlbarUtils.stripUnsafeProtocolOnPaste(pasteData);

    if (originalPasteData != pasteData) {
      // Unfortunately we're not allowed to set the bits being pasted
      // so cancel this event:
      event.preventDefault();
      event.stopImmediatePropagation();

      this.inputField.value = oldStart + pasteData + oldEnd;
      this._untrimmedValue = this.inputField.value;

      // Fix up cursor/selection:
      let newCursorPos = oldStart.length + pasteData.length;
      this.selectionStart = newCursorPos;
      this.selectionEnd = newCursorPos;
    }
  }

  _on_scrollend(event) {
    this._updateTextOverflow();
  }

  _on_TabSelect(event) {
    this._gotTabSelect = true;
    this._afterTabSelectAndFocusChange();
  }

  _on_keydown(event) {
    if (event.keyCode === KeyEvent.DOM_VK_RETURN) {
      if (this._keyDownEnterDeferred) {
        this._keyDownEnterDeferred.reject();
      }
      this._keyDownEnterDeferred = PromiseUtils.defer();
      event._disableCanonization = this._isKeyDownWithCtrl;
    } else if (event.keyCode !== KeyEvent.DOM_VK_CONTROL && event.ctrlKey) {
      this._isKeyDownWithCtrl = true;
    }

    // Due to event deferring, it's possible preventDefault() won't be invoked
    // soon enough to actually prevent some of the default behaviors, thus we
    // have to handle the event "twice". This first immediate call passes false
    // as second argument so that handleKeyNavigation will only simulate the
    // event handling, without actually executing actions.
    // TODO (Bug 1541806): improve this handling, maybe by delaying actions
    // instead of events.
    if (this.eventBufferer.shouldDeferEvent(event)) {
      this.controller.handleKeyNavigation(event, false);
    }
    this._toggleActionOverride(event);
    this.eventBufferer.maybeDeferEvent(event, () => {
      this.controller.handleKeyNavigation(event);
    });
  }

  async _on_keyup(event) {
    if (
      event.keyCode === KeyEvent.DOM_VK_RETURN &&
      this._keyDownEnterDeferred
    ) {
      try {
        const loadingBrowser = await this._keyDownEnterDeferred.promise;
        // Ensure the selected browser didn't change in the meanwhile.
        if (this.window.gBrowser.selectedBrowser === loadingBrowser) {
          loadingBrowser.focus();
          // Make sure the domain name stays visible for spoof protection and usability.
          this.selectionStart = this.selectionEnd = 0;
        }
        this._keyDownEnterDeferred = null;
      } catch (ex) {
        // Not all the Enter actions in the urlbar will cause a navigation, then it
        // is normal for this to be rejected.
        // If _keyDownEnterDeferred was rejected on keydown, we don't nullify it here
        // to ensure not overwriting the new value created by keydown.
      }
      return;
    } else if (event.keyCode === KeyEvent.DOM_VK_CONTROL) {
      this._isKeyDownWithCtrl = false;
    }

    this._toggleActionOverride(event);
  }

  _on_compositionstart(event) {
    if (this._compositionState == UrlbarUtils.COMPOSITION.COMPOSING) {
      throw new Error("Trying to start a nested composition?");
    }
    this._compositionState = UrlbarUtils.COMPOSITION.COMPOSING;

    if (UrlbarPrefs.get("keepPanelOpenDuringImeComposition")) {
      return;
    }

    // Close the view. This will also stop searching.
    if (this.view.isOpen) {
      // We're closing the view, but we want to retain search mode if the
      // selected result was previewing it.
      if (this.searchMode) {
        // If we entered search mode with an empty string, clear userTypedValue,
        // otherwise confirmSearchMode may try to set it as value.
        // This can happen for example if we entered search mode typing a
        // a partial engine domain and selecting a tab-to-search result.
        if (!this.value) {
          this.window.gBrowser.userTypedValue = null;
        }
        this.confirmSearchMode();
      }
      this._compositionClosedPopup = true;
      this.view.close();
    } else {
      this._compositionClosedPopup = false;
    }
  }

  _on_compositionend(event) {
    if (this._compositionState != UrlbarUtils.COMPOSITION.COMPOSING) {
      throw new Error("Trying to stop a non existing composition?");
    }

    if (!UrlbarPrefs.get("keepPanelOpenDuringImeComposition")) {
      // Clear the selection and the cached result, since they refer to the
      // state before this composition. A new input even will be generated
      // after this.
      this.view.clearSelection();
      this._resultForCurrentValue = null;
    }

    // We can't yet retrieve the committed value from the editor, since it isn't
    // completely committed yet. We'll handle it at the next input event.
    this._compositionState = event.data
      ? UrlbarUtils.COMPOSITION.COMMIT
      : UrlbarUtils.COMPOSITION.CANCELED;
  }

  _on_dragstart(event) {
    // Drag only if the gesture starts from the input field.
    let nodePosition = this.inputField.compareDocumentPosition(
      event.originalTarget
    );
    if (
      event.target != this.inputField &&
      !(nodePosition & Node.DOCUMENT_POSITION_CONTAINED_BY)
    ) {
      return;
    }

    // Don't cover potential drop targets on the toolbars or in content.
    this.view.close();

    // Only customize the drag data if the entire value is selected and it's a
    // loaded URI. Use default behavior otherwise.
    if (
      this.selectionStart != 0 ||
      this.selectionEnd != this.inputField.textLength ||
      this.getAttribute("pageproxystate") != "valid"
    ) {
      return;
    }

    let href = this.window.gBrowser.currentURI.displaySpec;
    let title = this.window.gBrowser.contentTitle || href;

    event.dataTransfer.setData("text/x-moz-url", `${href}\n${title}`);
    event.dataTransfer.setData("text/unicode", href);
    event.dataTransfer.setData("text/html", `<a href="${href}">${title}</a>`);
    event.dataTransfer.effectAllowed = "copyLink";
    event.stopPropagation();
  }

  _on_dragover(event) {
    if (!getDroppableData(event)) {
      event.dataTransfer.dropEffect = "none";
    }
  }

  _on_drop(event) {
    let droppedItem = getDroppableData(event);
    let droppedURL =
      droppedItem instanceof URL ? droppedItem.href : droppedItem;
    if (droppedURL && droppedURL !== this.window.gBrowser.currentURI.spec) {
      let principal = Services.droppedLinkHandler.getTriggeringPrincipal(event);
      this.value = droppedURL;
      this.setPageProxyState("invalid");
      this.focus();
      // To simplify tracking of events, register an initial event for event
      // telemetry, to replace the missing input event.
      this.controller.engagementEvent.start(event);
      this.handleNavigation({ triggeringPrincipal: principal });
      // For safety reasons, in the drop case we don't want to immediately show
      // the the dropped value, instead we want to keep showing the current page
      // url until an onLocationChange happens.
      // See the handling in `setURI` for further details.
      this.window.gBrowser.userTypedValue = null;
      this.setURI(null, true);
    }
  }

  _on_customizationstarting() {
    this.blur();

    this.inputField.controllers.removeController(this._copyCutController);
    delete this._copyCutController;
  }

  _on_aftercustomization() {
    this._initCopyCutController();
    this._initPasteAndGo();
  }
}

/**
 * Tries to extract droppable data from a DND event.
 * @param {Event} event The DND event to examine.
 * @returns {URL|string|null}
 *          null if there's a security reason for which we should do nothing.
 *          A URL object if it's a value we can load.
 *          A string value otherwise.
 */
function getDroppableData(event) {
  let links;
  try {
    links = Services.droppedLinkHandler.dropLinks(event);
  } catch (ex) {
    // This is either an unexpected failure or a security exception; in either
    // case we should always return null.
    return null;
  }
  // The URL bar automatically handles inputs with newline characters,
  // so we can get away with treating text/x-moz-url flavours as text/plain.
  if (links.length && links[0].url) {
    event.preventDefault();
    let href = links[0].url;
    if (UrlbarUtils.stripUnsafeProtocolOnPaste(href) != href) {
      // We may have stripped an unsafe protocol like javascript: and if so
      // there's no point in handling a partial drop.
      event.stopImmediatePropagation();
      return null;
    }

    try {
      // If this throws, checkLoadURStrWithPrincipal would also throw,
      // as that's what it does with things that don't pass the IO
      // service's newURI constructor without fixup. It's conceivable we
      // may want to relax this check in the future (so e.g. www.foo.com
      // gets fixed up), but not right now.
      let url = new URL(href);
      // If we succeed, try to pass security checks. If this works, return the
      // URL object. If the *security checks* fail, return null.
      try {
        let principal = Services.droppedLinkHandler.getTriggeringPrincipal(
          event
        );
        Services.scriptSecurityManager.checkLoadURIStrWithPrincipal(
          principal,
          url.href,
          Ci.nsIScriptSecurityManager.DISALLOW_INHERIT_PRINCIPAL
        );
        return url;
      } catch (ex) {
        return null;
      }
    } catch (ex) {
      // We couldn't make a URL out of this. Continue on, and return text below.
    }
  }
  // Handle as text.
  return event.dataTransfer.getData("text/unicode");
}

/**
 * Decodes the given URI for displaying it in the address bar without losing
 * information, such that hitting Enter again will load the same URI.
 *
 * @param {nsIURI} aURI
 *   The URI to decode
 * @returns {string}
 *   The decoded URI
 */
function losslessDecodeURI(aURI) {
  let scheme = aURI.scheme;
  let value = aURI.displaySpec;

  // Try to decode as UTF-8 if there's no encoding sequence that we would break.
  if (!/%25(?:3B|2F|3F|3A|40|26|3D|2B|24|2C|23)/i.test(value)) {
    let decodeASCIIOnly = !["https", "http", "file", "ftp"].includes(scheme);
    if (decodeASCIIOnly) {
      // This only decodes ascii characters (hex) 20-7e, except 25 (%).
      // This avoids both cases stipulated below (%-related issues, and \r, \n
      // and \t, which would be %0d, %0a and %09, respectively) as well as any
      // non-US-ascii characters.
      value = value.replace(
        /%(2[0-4]|2[6-9a-f]|[3-6][0-9a-f]|7[0-9a-e])/g,
        decodeURI
      );
    } else {
      try {
        value = decodeURI(value)
          // decodeURI decodes %25 to %, which creates unintended encoding
          // sequences. Re-encode it, unless it's part of a sequence that
          // survived decodeURI, i.e. one for:
          // ';', '/', '?', ':', '@', '&', '=', '+', '$', ',', '#'
          // (RFC 3987 section 3.2)
          .replace(
            /%(?!3B|2F|3F|3A|40|26|3D|2B|24|2C|23)/gi,
            encodeURIComponent
          );
      } catch (e) {}
    }
  }

  // Encode potentially invisible characters:
  //   U+0000-001F: C0/C1 control characters
  //   U+007F-009F: commands
  //   U+00A0, U+1680, U+2000-200A, U+202F, U+205F, U+3000: other spaces
  //   U+2028-2029: line and paragraph separators
  //   U+2800: braille empty pattern
  //   U+FFFC: object replacement character
  // Encode any trailing whitespace that may be part of a pasted URL, so that it
  // doesn't get eaten away by the location bar (bug 410726).
  // Encode all adjacent space chars (U+0020), to prevent spoofing attempts
  // where they would push part of the URL to overflow the location bar
  // (bug 1395508). A single space, or the last space if the are many, is
  // preserved to maintain readability of certain urls. We only do this for the
  // common space, because others may be eaten when copied to the clipboard, so
  // it's safer to preserve them encoded.
  value = value.replace(
    // eslint-disable-next-line no-control-regex
    /[\u0000-\u001f\u007f-\u00a0\u1680\u2000-\u200a\u2028\u2029\u202f\u205f\u2800\u3000\ufffc]|[\r\n\t]|\u0020(?=\u0020)|\s$/g,
    encodeURIComponent
  );

  // Encode characters that are ignorable, can't be rendered usefully, or may
  // confuse users.
  //
  // Default ignorable characters; ZWNJ (U+200C) and ZWJ (U+200D) are excluded
  // per bug 582186:
  //   U+00AD, U+034F, U+06DD, U+070F, U+115F-1160, U+17B4, U+17B5, U+180B-180E,
  //   U+2060, U+FEFF, U+200B, U+2060-206F, U+3164, U+FE00-FE0F, U+FFA0,
  //   U+FFF0-FFFB, U+1D173-1D17A (U+D834 + DD73-DD7A),
  //   U+E0000-E0FFF (U+DB40-DB43 + U+DC00-DFFF)
  // Bidi control characters (RFC 3987 sections 3.2 and 4.1 paragraph 6):
  //   U+061C, U+200E, U+200F, U+202A-202E, U+2066-2069
  // Other format characters in the Cf category that are unlikely to be rendered
  // usefully:
  //   U+0600-0605, U+08E2, U+110BD (U+D804 + U+DCBD),
  //   U+110CD (U+D804 + U+DCCD), U+13430-13438 (U+D80D + U+DC30-DC38),
  //   U+1BCA0-1BCA3 (U+D82F + U+DCA0-DCA3)
  // Mimicking UI parts:
  //   U+1F50F-1F513 (U+D83D + U+DD0F-DD13), U+1F6E1 (U+D83D + U+DEE1)
  value = value.replace(
    // eslint-disable-next-line no-misleading-character-class
    /[\u00ad\u034f\u061c\u06dd\u070f\u115f\u1160\u17b4\u17b5\u180b-\u180e\u200b\u200e\u200f\u202a-\u202e\u2060-\u206f\u3164\u0600-\u0605\u08e2\ufe00-\ufe0f\ufeff\uffa0\ufff0-\ufffb]|\ud804[\udcbd\udccd]|\ud80d[\udc30-\udc38]|\ud82f[\udca0-\udca3]|\ud834[\udd73-\udd7a]|[\udb40-\udb43][\udc00-\udfff]|\ud83d[\udd0f-\udd13\udee1]/g,
    encodeURIComponent
  );
  return value;
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
      urlbar.inputField.value =
        urlbar.inputField.value.substring(0, start) +
        urlbar.inputField.value.substring(end);
      urlbar.selectionStart = urlbar.selectionEnd = start;

      let event = urlbar.document.createEvent("UIEvents");
      event.initUIEvent("input", true, false, urlbar.window, 0);
      urlbar.inputField.dispatchEvent(event);
    }

    ClipboardHelper.copyString(val);
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
    return (
      this.supportsCommand(command) &&
      (command != "cmd_cut" || !this.urlbar.readOnly) &&
      this.urlbar.selectionStart < this.urlbar.selectionEnd
    );
  }

  onEvent() {}
}

/**
 * Manages the Add Search Engine contextual menu entries.
 */
class AddSearchEngineHelper {
  constructor(input) {
    this.document = input.document;
    let contextMenu = input.querySelector("moz-input-box").menupopup;
    this.separator = this.document.createXULElement("menuseparator");
    this.separator.setAttribute("anonid", "add-engine-separator");
    this.separator.classList.add("menuseparator-add-engine");
    this.separator.collapsed = true;
    contextMenu.appendChild(this.separator);

    // Since the contextual menu is not opened often, compared to the urlbar
    // results panel, we update it just before showing it, instead of spending
    // time on every page load.
    contextMenu.addEventListener("popupshowing", event => {
      // Ignore sub-menus.
      if (event.target == event.currentTarget) {
        this._refreshContextMenu();
      }
    });

    XPCOMUtils.defineLazyGetter(this, "_bundle", () =>
      Services.strings.createBundle("chrome://browser/locale/search.properties")
    );

    // Note: setEnginesFromBrowser must be invoked from the outside when the
    // page provided engines list changes.
  }

  /**
   * If there's more than this number of engines, the context menu offers
   * them in a submenu.
   */
  get maxInlineEngines() {
    return 3;
  }

  setEnginesFromBrowser(browser) {
    this.engines = browser.engines;
    this.browsingContext = browser.browsingContext;
  }

  _createMenuitem(engine, index) {
    let elt = this.document.createXULElement("menuitem");
    elt.setAttribute("anonid", `add-engine-${index}`);
    elt.classList.add("menuitem-iconic");
    elt.classList.add("context-menu-add-engine");
    elt.setAttribute(
      "label",
      this._bundle.formatStringFromName("cmd_addFoundEngine", [engine.title])
    );
    elt.setAttribute("uri", engine.uri);
    if (engine.icon) {
      elt.setAttribute("image", engine.icon);
    } else {
      elt.removeAttribute("image", engine.icon);
    }
    elt.addEventListener("command", this._onCommand.bind(this));
    return elt;
  }

  _createMenu(engine) {
    let elt = this.document.createXULElement("menu");
    elt.setAttribute("anonid", "add-engine-menu");
    elt.classList.add("menu-iconic");
    elt.classList.add("context-menu-add-engine");
    elt.setAttribute(
      "label",
      this._bundle.GetStringFromName("cmd_addFoundEngineMenu")
    );
    if (engine.icon) {
      elt.setAttribute("image", engine.icon);
    }
    let popup = this.document.createXULElement("menupopup");
    elt.appendChild(popup);
    return elt;
  }

  _refreshContextMenu() {
    let engines = this.engines || [];
    this.separator.collapsed = !engines.length;
    let curElt = this.separator;
    // Remove the previous items, if any.
    for (let elt = curElt.nextElementSibling; elt; ) {
      let nextElementSibling = elt.nextElementSibling;
      elt.remove();
      elt = nextElementSibling;
    }

    // If the page provides too many engines, we only show a single menu entry
    // with engines in a submenu.
    if (engines.length > this.maxInlineEngines) {
      // Set the menu button's image to the image of the first engine.  The
      // offered engines may have differing images, so there's no perfect
      // choice here.
      let elt = this._createMenu(engines[0]);
      this.separator.insertAdjacentElement("afterend", elt);
      curElt = elt.lastElementChild;
    }

    // Insert the engines, either in the contextual menu or the sub menu.
    for (let i = 0; i < engines.length; ++i) {
      let elt = this._createMenuitem(engines[i], i);
      if (curElt.localName == "menupopup") {
        curElt.appendChild(elt);
      } else {
        curElt.insertAdjacentElement("afterend", elt);
      }
      curElt = elt;
    }
  }

  _onCommand(event) {
    let uri = event.target.getAttribute("uri");
    let image = event.target.getAttribute("image");
    SearchUIUtils.addOpenSearchEngine(uri, image, this.browsingContext)
      .then(added => {
        if (added) {
          // Remove the offered engine from the list. The browser updated the
          // engines list at this point, so we just have to refresh the menu.)
          this._refreshContextMenu();
        }
      })
      .catch(console.error);
  }
}
