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
  BrowserUtils: "resource://gre/modules/BrowserUtils.jsm",
  ExtensionSearchHandler: "resource://gre/modules/ExtensionSearchHandler.jsm",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.jsm",
  ReaderMode: "resource://gre/modules/ReaderMode.jsm",
  Services: "resource://gre/modules/Services.jsm",
  UrlbarController: "resource:///modules/UrlbarController.jsm",
  UrlbarEventBufferer: "resource:///modules/UrlbarEventBufferer.jsm",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.jsm",
  UrlbarProvidersManager: "resource:///modules/UrlbarProvidersManager.jsm",
  UrlbarQueryContext: "resource:///modules/UrlbarUtils.jsm",
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

const SEARCH_BUTTON_ID = "urlbar-search-button";

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
    this.window.addEventListener("unload", this);

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

    this.searchButton = UrlbarPrefs.get("update2.searchButton");
    if (this.searchButton) {
      this.textbox.classList.add("searchButton");
    }

    this.controller = new UrlbarController({
      input: this,
      eventTelemetryCategory: options.eventTelemetryCategory,
    });
    this.view = new UrlbarView(this);
    this.valueIsTyped = false;
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
          return (this.inputField[property] = val);
        },
      });
    }

    this.inputField = this.querySelector("#urlbar-input");
    this._inputContainer = this.querySelector("#urlbar-input-container");
    this._identityBox = this.querySelector("#identity-box");
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
    this.textbox.addEventListener("mousedown", this);
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

    // Tracks IME composition.
    this._compositionState = UrlbarUtils.COMPOSITION.NONE;
    this._compositionClosedPopup = false;

    this.editor.newlineHandling =
      Ci.nsIEditor.eNewlinesStripSurroundingWhitespace;

    this._setOpenViewOnFocus();
    Services.prefs.addObserver("browser.urlbar.openViewOnFocus", this);
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
   * @param {boolean} [updatePopupNotifications]
   *        Passed though to `setPageProxyState`.
   */
  setURI(uri, updatePopupNotifications) {
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
        BrowserUtils.checkEmptyPageOrigin(
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
      BrowserUtils.checkEmptyPageOrigin(this.window.gBrowser.selectedBrowser)
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

    this.setPageProxyState(
      valid ? "valid" : "invalid",
      updatePopupNotifications
    );
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

  observe(subject, topic, data) {
    switch (data) {
      case "browser.urlbar.openViewOnFocus":
        this._setOpenViewOnFocus();
        break;
    }
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
   * Handles an event which would cause a url or text to be opened.
   *
   * @param {Event} [event] The event triggering the open.
   * @param {string} [openWhere] Where we expect the result to be opened.
   * @param {object} [openParams]
   *   The parameters related to where the result will be opened.
   * @param {object} [triggeringPrincipal]
   *   The principal that the action was triggered from.
   */
  handleCommand(event, openWhere, openParams = {}, triggeringPrincipal = null) {
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
    let selectedOneOff;
    if (this.view.isOpen) {
      selectedOneOff = this.view.oneOffSearchButtons.selectedButton;
      if (selectedOneOff && isMouseEvent && event.target != selectedOneOff) {
        selectedOneOff = null;
      }
      // Do the command of the selected one-off if it's not an engine.
      if (selectedOneOff && !selectedOneOff.engine) {
        this.controller.engagementEvent.discard();
        selectedOneOff.doCommand();
        return;
      }
    }

    // Use the selected element if we have one; this is usually the case
    // when the view is open.
    let element = this.view.selectedElement;
    let result = this.view.getResultFromElement(element);
    let selectedPrivateResult =
      result &&
      result.type == UrlbarUtils.RESULT_TYPE.SEARCH &&
      result.payload.inPrivateWindow;
    let selectedPrivateEngineResult =
      selectedPrivateResult && result.payload.isPrivateEngine;
    if (element && (!selectedOneOff || selectedPrivateEngineResult)) {
      this.pickElement(element, event);
      return;
    }

    let url;
    let selType = this.controller.engagementEvent.typeFromElement(element);
    let numChars = this.value.length;
    if (selectedOneOff) {
      selType = "oneoff";
      numChars = this._lastSearchString.length;
      // If there's a selected one-off button then load a search using
      // the button's engine.
      result = this._resultForCurrentValue;
      let searchString =
        (result && (result.payload.suggestion || result.payload.query)) ||
        this._lastSearchString;
      [url, openParams.postData] = UrlbarUtils.getSearchQueryUrl(
        selectedOneOff.engine,
        searchString
      );
      this._recordSearch(selectedOneOff.engine, event);
    } else {
      // Use the current value if we don't have a UrlbarResult e.g. because the
      // view is closed.
      url = this.untrimmedValue;
      openParams.postData = null;
    }

    if (!url) {
      return;
    }

    this.controller.recordSelectedResult(
      event,
      result || this.view.selectedResult
    );

    let where = openWhere || this._whereToOpen(event);
    if (selectedPrivateResult) {
      where = "window";
      openParams.private = true;
    }
    openParams.allowInheritPrincipal = false;
    url = this._maybeCanonizeURL(event, url) || url.trim();

    this.controller.engagementEvent.record(event, {
      numChars,
      selIndex: this.view.selectedRowIndex,
      selType,
    });

    try {
      new URL(url);
    } catch (ex) {
      // This is not a URL, so it must be a search or a keyword.

      // TODO (Bug 1604927): If the urlbar results are restricted to a specific
      // engine, here we must search with that specific engine; indeed the
      // docshell wouldn't know about our engine restriction.
      // Also remember to invoke this._recordSearch, after replacing url with
      // the appropriate engine submission url.

      let browser = this.window.gBrowser.selectedBrowser;
      let lastLocationChange = browser.lastLocationChange;
      UrlbarUtils.getShortcutOrURIAndPostData(url).then(data => {
        // Because this happens asynchronously, we must verify that the browser
        // location did not change in the meanwhile.
        if (
          where != "current" ||
          browser.lastLocationChange == lastLocationChange
        ) {
          openParams.postData = data.postData;
          openParams.allowInheritPrincipal = data.mayInheritPrincipal;
          this._loadURL(data.url, where, openParams, null, browser);
        }
      });
      // Bail out, because we will handle the _loadURL call asynchronously.
      return;
    }

    this._loadURL(url, where, openParams);
  }

  handleRevert() {
    this.window.gBrowser.userTypedValue = null;
    this.setURI(null, true);
    if (this.value && this.focused) {
      this.select();
    }
  }

  /**
   * Called by the view when an element is picked.
   *
   * @param {Element} element The element that was picked.
   * @param {Event} event The event that picked the element.
   */
  pickElement(element, event) {
    let originalUntrimmedValue = this.untrimmedValue;
    let result = this.view.getResultFromElement(element);
    if (!result) {
      return;
    }
    let isCanonized = this.setValueFromResult(result, event);
    let where = this._whereToOpen(event);
    let openParams = {
      allowInheritPrincipal: false,
    };

    let selIndex = result.rowIndex;
    if (!result.payload.keywordOffer) {
      this.view.close(/* elementPicked */ true);
    }

    this.controller.recordSelectedResult(event, result);

    if (isCanonized) {
      this.controller.engagementEvent.record(event, {
        numChars: this._lastSearchString.length,
        selIndex,
        selType: "canonized",
      });
      this._loadURL(this.value, where, openParams);
      return;
    }

    let { url, postData } = UrlbarUtils.getUrlFromResult(result);
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
          numChars: this._lastSearchString.length,
          selIndex,
          selType: "tabswitch",
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
        if (result.payload.keywordOffer) {
          // The user confirmed a token alias, so just move the caret
          // to the end of it. Because there's a trailing space in the value,
          // the user can directly start typing a query string at that point.
          this.selectionStart = this.selectionEnd = this.value.length;

          this.controller.engagementEvent.record(event, {
            numChars: this._lastSearchString.length,
            selIndex,
            selType: "keywordoffer",
          });

          // Picking a keyword offer just fills it in the input and doesn't
          // visit anything.  The user can then type a search string.  Also
          // start a new search so that the offer appears in the view by itself
          // to make it even clearer to the user what's going on.
          this.startQuery();
          return;
        }

        if (
          result.heuristic &&
          this.window.gKeywordURIFixup &&
          UrlbarUtils.looksLikeSingleWordHost(originalUntrimmedValue)
        ) {
          // When fixing a single word to a search, the docShell also checks the
          // DNS and asks the user whether they would rather visit that as a
          // host. On a positive answer, it adds to the domain whitelist that
          // we use to make decisions. Because we are directly asking for a
          // search here, bypassing the docShell, we need invoke the same check
          // ourselves. See also URIFixupChild.jsm and keyword-uri-fixup.
          let flags =
            Ci.nsIURIFixup.FIXUP_FLAG_FIX_SCHEME_TYPOS |
            Ci.nsIURIFixup.FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP;
          if (PrivateBrowsingUtils.isWindowPrivate(this.window)) {
            flags |= Ci.nsIURIFixup.FIXUP_FLAG_PRIVATE_CONTEXT;
          }
          // Don't interrupt the load action in case of errors.
          try {
            let fixupInfo = Services.uriFixup.getFixupURIInfo(
              originalUntrimmedValue.trim(),
              flags
            );
            this.window.gKeywordURIFixup.check(
              this.window.gBrowser.selectedBrowser,
              fixupInfo
            );
          } catch (ex) {
            Cu.reportError(
              `An error occured while trying to fixup "${originalUntrimmedValue.trim()}": ${ex}`
            );
          }
        }

        if (result.payload.inPrivateWindow) {
          where = "window";
          openParams.private = true;
        }

        const actionDetails = {
          isSuggestion: !!result.payload.suggestion,
          alias: result.payload.keyword,
        };
        const engine = Services.search.getEngineByName(result.payload.engine);
        this._recordSearch(engine, event, actionDetails);
        break;
      }
      case UrlbarUtils.RESULT_TYPE.TIP: {
        let scalarName;
        if (element.classList.contains("urlbarView-tip-help")) {
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
            numChars: this._lastSearchString.length,
            selIndex,
            selType: "tip",
          });
          let provider = UrlbarProvidersManager.getProvider(
            result.providerName
          );
          if (!provider) {
            Cu.reportError(`Provider not found: ${result.providerName}`);
            return;
          }
          provider.tryMethod("pickResult", result);
          return;
        }
        break;
      }
      case UrlbarUtils.RESULT_TYPE.OMNIBOX: {
        this.controller.engagementEvent.record(event, {
          numChars: this._lastSearchString.length,
          selIndex,
          selType: "extension",
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
      numChars: this._lastSearchString.length,
      selIndex,
      selType: this.controller.engagementEvent.typeFromElement(element),
    });

    this._loadURL(url, where, openParams, {
      source: result.source,
      type: result.type,
    });
  }

  /**
   * Called by the view when moving through results with the keyboard, and when
   * picking a result.
   *
   * @param {UrlbarResult} [result]
   *   The result that was selected or picked, null if no result was selected.
   * @param {Event} [event] The event that picked the result.
   * @returns {boolean}
   *   Whether the value has been canonized
   */
  setValueFromResult(result = null, event = null) {
    let canonizedUrl;

    // Usually this is set by a previous input event, but in certain cases, like
    // when opening Top Sites on a loaded page, it wouldn't happen. To avoid
    // confusing the user, we always enforce it when a result changes our value.
    this.setPageProxyState("invalid", true);

    if (!result) {
      // This happens when there's no selection, for example when moving to the
      // one-offs search settings button, or to the input field when Top Sites
      // are shown; then we must reset the input value.
      // Note that for Top Sites the last search string would be empty, thus we
      // must restore the last text value.
      this.value = this._lastSearchString || this._valueOnLastSearch;
    } else {
      // For autofilled results, the value that should be canonized is not the
      // autofilled value but the value that the user typed.
      canonizedUrl = this._maybeCanonizeURL(
        event,
        result.autofill ? this._lastSearchString : this.value
      );
      if (canonizedUrl) {
        this.value = canonizedUrl;
      } else if (result.autofill) {
        let { value, selectionStart, selectionEnd } = result.autofill;
        this._autofillValue(value, selectionStart, selectionEnd);
      } else {
        this.value = this._getValueFromResult(result);
      }
    }
    this._resultForCurrentValue = result;

    // The value setter clobbers the actiontype attribute, so update this after
    // that.
    if (result) {
      switch (result.type) {
        case UrlbarUtils.RESULT_TYPE.TAB_SWITCH:
          this.setAttribute("actiontype", "switchtab");
          break;
        case UrlbarUtils.RESULT_TYPE.OMNIBOX:
          this.setAttribute("actiontype", "extension");
          break;
      }
    }

    return !!canonizedUrl;
  }

  /**
   * Called by the controller when the first result of a new search is received.
   * If it's an autofill result, then it may need to be autofilled, subject to a
   * few restrictions.
   *
   * @param {UrlbarResult} result
   *   The first result.
   */
  autofillFirstResult(result) {
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

    this.setValueFromResult(result);
  }

  /**
   * Invoked by the view when the first result is received.
   * To prevent selection flickering, we apply autofill on input through a
   * placeholder, without waiting for results.
   * But, if the first result is not an autofill one, the autofill prediction
   * was wrong and we should restore the original user typed string.
   * @param {UrlbarResult} firstResult The first result received.
   */
  maybeClearAutofillPlaceholder(firstResult) {
    if (
      this._autofillPlaceholder &&
      !firstResult.autofill &&
      // Avoid clobbering added spaces (for token aliases, for example).
      !this.value.endsWith(" ")
    ) {
      this._setValue(this.window.gBrowser.userTypedValue, false);
    }
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

    // TODO (Bug 1522902): This promise is necessary for tests, because some
    // tests are not listening for completion when starting a query through
    // other methods than startQuery (input events for example).
    this.lastQueryContextPromise = this.controller.startQuery(
      new UrlbarQueryContext({
        allowAutofill,
        isPrivate: this.isPrivate,
        maxResults: UrlbarPrefs.get("maxRichResults"),
        searchString,
        userContextId: this.window.gBrowser.selectedBrowser.getAttribute(
          "usercontextid"
        ),
        currentPage: this.window.gBrowser.currentURI.spec,
        allowSearchSuggestions:
          !event ||
          !UrlbarUtils.isPasteEvent(event) ||
          !event.data ||
          event.data.length <= UrlbarPrefs.get("maxCharsForSearchSuggestions"),
      })
    );
  }

  /**
   * Sets the input's value, starts a search, and opens the view.
   *
   * @param {string} value
   *   The input's value will be set to this value, and the search will
   *   use it as its query.
   * @param {boolean} [options.focus]
   *   If true, the urlbar will be focused.  If false, the focus will remain
   *   unchanged.
   */
  search(value, { focus = true } = {}) {
    if (focus) {
      this.focus();
    }

    // If the value is a restricted token, append a space.
    if (Object.values(UrlbarTokenizer.RESTRICT).includes(value)) {
      this.inputField.value = value + " ";
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
      this.startLayoutExtend();
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
    return this._setValue(val, true);
  }

  get lastSearchString() {
    return this._lastSearchString;
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
    this.startLayoutExtend();
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
    // The Urlbar is unfocused and the view is closed
    if (this.getAttribute("focused") != "true" && !this.view.isOpen) {
      return;
    }

    if (UrlbarPrefs.get("disableExtendForTests")) {
      this.setAttribute("breakout-extend-disabled", "true");
      return;
    }
    this.removeAttribute("breakout-extend-disabled");

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
    if (
      !this.hasAttribute("breakout-extend") ||
      this.view.isOpen ||
      this.getAttribute("focused") == "true"
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

  // Private methods below.

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

  _setOpenViewOnFocus() {
    // FIXME: Not using UrlbarPrefs because its pref observer may run after
    // this call, so we'd get the previous openViewOnFocus value here. This
    // can be cleaned up after bug 1560013.
    this.openViewOnFocus = Services.prefs.getBoolPref(
      "browser.urlbar.openViewOnFocus"
    );
  }

  _setValue(val, allowTrim) {
    this._untrimmedValue = val;

    let originalUrl = ReaderMode.getOriginalUrlObjectForDisplay(val);
    if (originalUrl) {
      val = originalUrl.displaySpec;
    }

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

  _getValueFromResult(result) {
    switch (result.type) {
      case UrlbarUtils.RESULT_TYPE.KEYWORD:
        return result.payload.input;
      case UrlbarUtils.RESULT_TYPE.SEARCH:
        return (
          (result.payload.keyword ? result.payload.keyword + " " : "") +
          (result.payload.suggestion || result.payload.query)
        );
      case UrlbarUtils.RESULT_TYPE.OMNIBOX:
        return result.payload.content;
    }

    try {
      let uri = Services.io.newURI(result.payload.url);
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
    let allowAutofill = this.selectionEnd == value.length;

    // Determine whether we can autofill the placeholder.  The placeholder is a
    // value that we autofill now, when the search starts and before we wait on
    // its first result, in order to prevent a flicker in the input caused by
    // the previous autofilled substring disappearing and reappearing when the
    // first result arrives.  Of course we can only autofill the placeholder if
    // it starts with the new search string, and we shouldn't autofill anything
    // if the caret isn't at the end of the input.
    if (
      !allowAutofill ||
      this._autofillPlaceholder.length <= value.length ||
      !this._autofillPlaceholder
        .toLocaleLowerCase()
        .startsWith(value.toLocaleLowerCase())
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

    // If the entire URL is selected, just use the actual loaded URI,
    // unless we want a decoded URI, or it's a data: or javascript: URI,
    // since those are hard to read when encoded.
    if (
      this.value == selectedVal &&
      !uri.schemeIs("javascript") &&
      !uri.schemeIs("data") &&
      !UrlbarPrefs.get("decodeURLsOnCopy")
    ) {
      return uri.displaySpec;
    }

    // Just the beginning of the URL is selected, or we want a decoded
    // url. First check for a trimmed value.
    let spec = uri.displaySpec;
    let trimmedSpec = this._trimValue(spec);
    if (spec != trimmedSpec) {
      // Prepend the portion that _trimValue removed from the beginning.
      // This assumes _trimValue will only truncate the URL at
      // the beginning or end (or both).
      let trimmedSegments = spec.split(trimmedSpec);
      selectedVal = trimmedSegments[0] + selectedVal;
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
   * @param {alias} searchActionDetails.alias
   *   True if this query was initiated via a search alias.
   */
  _recordSearch(engine, event, searchActionDetails = {}) {
    const isOneOff = this.view.oneOffSearchButtons.maybeRecordTelemetry(event);
    // Infer the type of the event which triggered the search.
    let eventType = "unknown";
    if (event instanceof KeyboardEvent) {
      eventType = "key";
    } else if (event instanceof MouseEvent) {
      eventType = "mouse";
    }
    // Augment the search action details object.
    let details = searchActionDetails;
    details.isOneOff = isOneOff;
    details.type = eventType;

    this.window.BrowserSearch.recordSearchInTelemetry(
      engine,
      "urlbar",
      details
    );
  }

  /**
   * Shortens the given value, usually by removing http:// and trailing slashes,
   * such that calling nsIURIFixup::createFixupURI with the result will produce
   * the same URI.
   *
   * @param {string} val
   *   The string to be trimmed if it appears to be URI
   * @returns {string}
   *   The trimmed string
   */
  _trimValue(val) {
    return UrlbarPrefs.get("trimURLs") ? BrowserUtils.trimURL(val) : val;
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
    value = "http://www." + value;

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
        this.window.SitePermissions.clearTemporaryPermissions(browser);
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

    // Focus the content area before triggering loads, since if the load
    // occurs in a new tab, we want focus to be restored to the content
    // area when the current tab is re-selected.
    browser.focus();

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

    // Make sure the domain name stays visible for spoof protection and usability.
    this.selectionStart = this.selectionEnd = 0;

    this.view.close();
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
    let label = Services.strings
      .createBundle("chrome://browser/locale/browser.properties")
      .GetStringFromName("pasteAndGo.label");
    pasteAndGo.setAttribute("label", label);
    pasteAndGo.setAttribute("anonid", "paste-and-go");
    pasteAndGo.addEventListener("command", () => {
      this._suppressStartQuery = true;

      this.select();
      this.window.goDoCommand("cmd_paste");
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
    // should not be recorded as an abandonment.
    this.controller.engagementEvent.discard();
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
      numChars: this._lastSearchString.length,
    });

    this.removeAttribute("focused");
    this.endLayoutExtend();

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
      // It doesn't really matter which search engine is used here, thus it's ok
      // to ignore whether we are in a private context. The keyword lookup will
      // also distinguish between whitelisted and not whitelisted hosts.
      let untrim = false;
      try {
        let fixedSpec = Services.uriFixup.createFixupURI(
          this.value,
          Services.uriFixup.FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP |
            Services.uriFixup.FIXUP_FLAG_FIX_SCHEME_TYPOS
        ).displaySpec;
        let expectedSpec = Services.io.newURI(this._untrimmedValue).displaySpec;
        untrim = fixedSpec != expectedSpec;
      } catch (ex) {
        untrim = true;
      }
      if (untrim) {
        this.inputField.value = this._focusUntrimmedValue = this._untrimmedValue;
      }
    }

    this.startLayoutExtend();

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
          this.view.autoOpen({ event });
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
    } else if (!value && !this.openViewOnFocus) {
      this.view.clear();
      this.view.close();
      return;
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
      compositionState == UrlbarUtils.COMPOSITION.COMPOSING ||
      (compositionState == UrlbarUtils.COMPOSITION.CANCELED &&
        !compositionClosedPopup)
    ) {
      return;
    }

    // Autofill only when text is inserted (i.e., event.data is not empty) and
    // it's not due to pasting.
    let allowAutofill =
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

    let pasteData = UrlbarUtils.stripUnsafeProtocolOnPaste(originalPasteData);
    if (originalPasteData != pasteData) {
      // Unfortunately we're not allowed to set the bits being pasted
      // so cancel this event:
      event.preventDefault();
      event.stopImmediatePropagation();

      this.inputField.value = oldStart + pasteData + oldEnd;
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

  _on_keyup(event) {
    this._toggleActionOverride(event);
  }

  _on_compositionstart(event) {
    if (this._compositionState == UrlbarUtils.COMPOSITION.COMPOSING) {
      throw new Error("Trying to start a nested composition?");
    }
    this._compositionState = UrlbarUtils.COMPOSITION.COMPOSING;

    // Close the view. This will also stop searching.
    if (this.view.isOpen) {
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
      this.handleCommand(null, undefined, undefined, principal);
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

  _on_unload() {
    // We can remove this once UrlbarPrefs has support for listeners.
    // (bug 1560013)
    Services.prefs.removeObserver("browser.urlbar.openViewOnFocus", this);
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
      // If this throws, urlSecurityCheck would also throw, as that's what it
      // does with things that don't pass the IO service's newURI constructor
      // without fixup. It's conceivable we may want to relax this check in
      // the future (so e.g. www.foo.com gets fixed up), but not right now.
      let url = new URL(href);
      // If we succeed, try to pass security checks. If this works, return the
      // URL object. If the *security checks* fail, return null.
      try {
        let principal = Services.droppedLinkHandler.getTriggeringPrincipal(
          event
        );
        BrowserUtils.urlSecurityCheck(
          url,
          principal,
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
          // 1. decodeURI decodes %25 to %, which creates unintended
          //    encoding sequences. Re-encode it, unless it's part of
          //    a sequence that survived decodeURI, i.e. one for:
          //    ';', '/', '?', ':', '@', '&', '=', '+', '$', ',', '#'
          //    (RFC 3987 section 3.2)
          // 2. Re-encode select whitespace so that it doesn't get eaten
          //    away by the location bar (bug 410726). Re-encode all
          //    adjacent whitespace, to prevent spoofing attempts where
          //    invisible characters would push part of the URL to
          //    overflow the location bar (bug 1395508).
          .replace(
            /%(?!3B|2F|3F|3A|40|26|3D|2B|24|2C|23)|[\r\n\t]|\s(?=\s)|\s$/gi,
            encodeURIComponent
          );
      } catch (e) {}
    }
  }

  // Encode invisible characters (C0/C1 control characters, U+007F [DEL],
  // U+00A0 [no-break space], line and paragraph separator,
  // object replacement character) (bug 452979, bug 909264)
  value = value.replace(
    // eslint-disable-next-line no-control-regex
    /[\u0000-\u001f\u007f-\u00a0\u2028\u2029\ufffc]/g,
    encodeURIComponent
  );

  // Encode default ignorable characters (bug 546013)
  // except ZWNJ (U+200C) and ZWJ (U+200D) (bug 582186).
  // This includes all bidirectional formatting characters.
  // (RFC 3987 sections 3.2 and 4.1 paragraph 6)
  value = value.replace(
    // eslint-disable-next-line no-misleading-character-class
    /[\u00ad\u034f\u061c\u115f-\u1160\u17b4-\u17b5\u180b-\u180d\u200b\u200e-\u200f\u202a-\u202e\u2060-\u206f\u3164\ufe00-\ufe0f\ufeff\uffa0\ufff0-\ufff8]|\ud834[\udd73-\udd7a]|[\udb40-\udb43][\udc00-\udfff]/g,
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
