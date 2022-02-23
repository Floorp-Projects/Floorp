/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* globals XULCommandEvent */

// This is loaded into chrome windows with the subscript loader. Wrap in
// a block to prevent accidentally leaking globals onto `window`.
{
  /**
   * Defines the search bar element.
   */
  class MozSearchbar extends MozXULElement {
    static get inheritedAttributes() {
      return {
        ".searchbar-textbox":
          "disabled,disableautocomplete,searchengine,src,newlines",
        ".searchbar-search-button": "addengines",
      };
    }

    static get markup() {
      return `
        <stringbundle src="chrome://browser/locale/search.properties"></stringbundle>
        <hbox class="searchbar-search-button" data-l10n-id="searchbar-icon">
          <image class="searchbar-search-icon"></image>
          <image class="searchbar-search-icon-overlay"></image>
        </hbox>
        <html:input class="searchbar-textbox" is="autocomplete-input" type="search" data-l10n-id="searchbar-input" autocompletepopup="PopupSearchAutoComplete" autocompletesearch="search-autocomplete" autocompletesearchparam="searchbar-history" maxrows="10" completeselectedindex="true" minresultsforpopup="0"/>
        <menupopup class="textbox-contextmenu"></menupopup>
        <hbox class="search-go-container">
          <image class="search-go-button urlbar-icon" hidden="true" onclick="handleSearchCommand(event);" data-l10n-id="searchbar-submit"></image>
        </hbox>
      `;
    }

    constructor() {
      super();

      MozXULElement.insertFTLIfNeeded("browser/search.ftl");

      this.destroy = this.destroy.bind(this);
      this._setupEventListeners();
      let searchbar = this;
      this.observer = {
        observe(aEngine, aTopic, aVerb) {
          if (
            aTopic == "browser-search-engine-modified" ||
            (aTopic == "browser-search-service" && aVerb == "init-complete")
          ) {
            // Make sure the engine list is refetched next time it's needed
            searchbar._engines = null;

            // Update the popup header and update the display after any modification.
            searchbar._textbox.popup.updateHeader();
            searchbar.updateDisplay();
          }
        },
        QueryInterface: ChromeUtils.generateQI(["nsIObserver"]),
      };

      this._ignoreFocus = false;
      this._engines = null;
      this.telemetrySelectedIndex = -1;
    }

    connectedCallback() {
      // Don't initialize if this isn't going to be visible
      if (this.closest("#BrowserToolbarPalette")) {
        return;
      }

      this.appendChild(this.constructor.fragment);
      this.initializeAttributeInheritance();

      // Don't go further if in Customize mode.
      if (this.parentNode.parentNode.localName == "toolbarpaletteitem") {
        return;
      }

      // Ensure we get persisted widths back, if we've been in the palette:
      let storedWidth = Services.xulStore.getValue(
        document.documentURI,
        this.parentNode.id,
        "width"
      );
      if (storedWidth) {
        this.parentNode.setAttribute("width", storedWidth);
      }

      this._stringBundle = this.querySelector("stringbundle");
      this._textbox = this.querySelector(".searchbar-textbox");

      this._menupopup = null;
      this._pasteAndSearchMenuItem = null;

      this._setupTextboxEventListeners();
      this._initTextbox();

      window.addEventListener("unload", this.destroy);

      this.FormHistory = ChromeUtils.import(
        "resource://gre/modules/FormHistory.jsm",
        {}
      ).FormHistory;
      this.SearchSuggestionController = ChromeUtils.import(
        "resource://gre/modules/SearchSuggestionController.jsm",
        {}
      ).SearchSuggestionController;

      Services.obs.addObserver(this.observer, "browser-search-engine-modified");
      Services.obs.addObserver(this.observer, "browser-search-service");

      this._initialized = true;

      (window.delayedStartupPromise || Promise.resolve()).then(() => {
        window.requestIdleCallback(() => {
          Services.search
            .init()
            .then(aStatus => {
              // Bail out if the binding's been destroyed
              if (!this._initialized) {
                return;
              }

              // Refresh the display (updating icon, etc)
              this.updateDisplay();
              BrowserSearch.updateOpenSearchBadge();
            })
            .catch(status =>
              Cu.reportError(
                "Cannot initialize search service, bailing out: " + status
              )
            );
        });
      });

      // Wait until the popupshowing event to avoid forcing immediate
      // attachment of the search-one-offs binding.
      this.textbox.popup.addEventListener(
        "popupshowing",
        () => {
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
        },
        { capture: true, once: true }
      );
    }

    async getEngines() {
      if (!this._engines) {
        this._engines = await Services.search.getVisibleEngines();
      }
      return this._engines;
    }

    set currentEngine(val) {
      if (PrivateBrowsingUtils.isWindowPrivate(window)) {
        Services.search.defaultPrivateEngine = val;
      } else {
        Services.search.defaultEngine = val;
      }
    }

    get currentEngine() {
      let currentEngine;
      if (PrivateBrowsingUtils.isWindowPrivate(window)) {
        currentEngine = Services.search.defaultPrivateEngine;
      } else {
        currentEngine = Services.search.defaultEngine;
      }
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
      this._textbox.value = val;
    }

    get value() {
      return this._textbox.value;
    }

    destroy() {
      if (this._initialized) {
        this._initialized = false;
        window.removeEventListener("unload", this.destroy);

        Services.obs.removeObserver(
          this.observer,
          "browser-search-engine-modified"
        );
        Services.obs.removeObserver(this.observer, "browser-search-service");
      }

      // Make sure to break the cycle from _textbox to us. Otherwise we leak
      // the world. But make sure it's actually pointing to us.
      // Also make sure the textbox has ever been constructed, otherwise the
      // _textbox getter will cause the textbox constructor to run, add an
      // observer, and leak the world too.
      if (
        this._textbox &&
        this._textbox.mController &&
        this._textbox.mController.input &&
        this._textbox.mController.input.wrappedJSObject ==
          this.nsIAutocompleteInput
      ) {
        this._textbox.mController.input = null;
      }
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
      this._textbox.title = this._stringBundle.getFormattedString("searchtip", [
        this.currentEngine.name,
      ]);
    }

    updateGoButtonVisibility() {
      this.querySelector(".search-go-button").hidden = !this._textbox.value;
    }

    openSuggestionsPanel(aShowOnlySettingsIfEmpty) {
      if (this._textbox.open) {
        return;
      }

      this._textbox.showHistoryPopup();

      if (this._textbox.value) {
        // showHistoryPopup does a startSearch("") call, ensure the
        // controller handles the text from the input box instead:
        this._textbox.mController.handleText();
      } else if (aShowOnlySettingsIfEmpty) {
        this.setAttribute("showonlysettings", "true");
      }
    }

    async selectEngine(aEvent, isNextEngine) {
      // Stop event bubbling now, because the rest of this method is async.
      aEvent.preventDefault();
      aEvent.stopPropagation();

      // Find the new index.
      let engines = await this.getEngines();
      let currentName = this.currentEngine.name;
      let newIndex = -1;
      let lastIndex = engines.length - 1;
      for (let i = lastIndex; i >= 0; --i) {
        if (engines[i].name == currentName) {
          // Check bounds to cycle through the list of engines continuously.
          if (!isNextEngine && i == 0) {
            newIndex = lastIndex;
          } else if (isNextEngine && i == lastIndex) {
            newIndex = 0;
          } else {
            newIndex = i + (isNextEngine ? 1 : -1);
          }
          break;
        }
      }

      this.currentEngine = engines[newIndex];

      this.openSuggestionsPanel();
    }

    handleSearchCommand(aEvent, aEngine, aForceNewTab) {
      let where = "current";
      let params;
      const newTabPref = Services.prefs.getBoolPref("browser.search.openintab");

      // Open ctrl/cmd clicks on one-off buttons in a new background tab.
      if (
        aEvent &&
        aEvent.originalTarget.classList.contains("search-go-button")
      ) {
        if (aEvent.button == 2) {
          return;
        }
        where = whereToOpenLink(aEvent, false, true);
        if (
          newTabPref &&
          !aEvent.altKey &&
          !aEvent.getModifierState("AltGraph") &&
          where == "current" &&
          !gBrowser.selectedTab.isEmpty
        ) {
          where = "tab";
        }
      } else if (aForceNewTab) {
        where = "tab";
        if (Services.prefs.getBoolPref("browser.tabs.loadInBackground")) {
          where += "-background";
        }
      } else {
        if (
          (aEvent instanceof KeyboardEvent &&
            (aEvent.altKey || aEvent.getModifierState("AltGraph"))) ^
            newTabPref &&
          !gBrowser.selectedTab.isEmpty
        ) {
          where = "tab";
        }
        if (
          aEvent instanceof MouseEvent &&
          (aEvent.button == 1 || aEvent.getModifierState("Accel"))
        ) {
          where = "tab";
          params = {
            inBackground: true,
          };
        }
      }
      this.handleSearchCommandWhere(aEvent, aEngine, where, params);
    }

    handleSearchCommandWhere(aEvent, aEngine, aWhere, aParams = {}) {
      let textBox = this._textbox;
      let textValue = textBox.value;

      let selectedIndex = this.telemetrySelectedIndex;
      let isOneOff = false;

      BrowserSearchTelemetry.recordSearchSuggestionSelectionMethod(
        aEvent,
        "searchbar",
        selectedIndex
      );

      if (selectedIndex == -1) {
        isOneOff = this.textbox.popup.oneOffButtons.eventTargetIsAOneOff(
          aEvent
        );
      }

      if (aWhere === "tab" && !!aParams.inBackground) {
        // Keep the focus in the search bar.
        aParams.avoidBrowserFocus = true;
      } else if (
        aWhere !== "window" &&
        aEvent.keyCode === KeyEvent.DOM_VK_RETURN
      ) {
        // Move the focus to the selected browser when keyup the Enter.
        aParams.avoidBrowserFocus = true;
        this._needBrowserFocusAtEnterKeyUp = true;
      }

      // This is a one-off search only if oneOffRecorded is true.
      this.doSearch(textValue, aWhere, aEngine, aParams, isOneOff);
    }

    doSearch(aData, aWhere, aEngine, aParams, isOneOff = false) {
      let textBox = this._textbox;
      let engine = aEngine || this.currentEngine;

      // Save the current value in the form history
      if (
        aData &&
        !PrivateBrowsingUtils.isWindowPrivate(window) &&
        this.FormHistory.enabled &&
        aData.length <=
          this.SearchSuggestionController.SEARCH_HISTORY_MAX_VALUE_LENGTH
      ) {
        this.FormHistory.update(
          {
            op: "bump",
            fieldname: textBox.getAttribute("autocompletesearchparam"),
            value: aData,
            source: engine.name,
          },
          {
            handleError(aError) {
              Cu.reportError(
                "Saving search to form history failed: " + aError.message
              );
            },
          }
        );
      }

      let submission = engine.getSubmission(aData, null, "searchbar");

      // If we hit here, we come either from a one-off, a plain search or a suggestion.
      const details = {
        isOneOff,
        isSuggestion: !isOneOff && this.telemetrySelectedIndex != -1,
        url: submission.uri,
      };

      this.telemetrySelectedIndex = -1;

      BrowserSearchTelemetry.recordSearch(
        gBrowser.selectedBrowser,
        engine,
        "searchbar",
        details
      );
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

    /**
     * Determines if we should select all the text in the searchbar based on the
     * searchbar state, and whether the selection is empty.
     */
    _maybeSelectAll() {
      if (
        !this._preventClickSelectsAll &&
        document.activeElement == this._textbox &&
        this._textbox.selectionStart == this._textbox.selectionEnd
      ) {
        this.select();
      }
    }

    _setupEventListeners() {
      this.addEventListener("click", event => {
        this._maybeSelectAll();
      });

      this.addEventListener(
        "DOMMouseScroll",
        event => {
          if (event.getModifierState("Accel")) {
            this.selectEngine(event, event.detail > 0);
          }
        },
        true
      );

      this.addEventListener("input", event => {
        this.updateGoButtonVisibility();
      });

      this.addEventListener("drop", event => {
        this.updateGoButtonVisibility();
      });

      this.addEventListener(
        "blur",
        event => {
          // Reset the flag since we can't capture enter keyup event if the event happens
          // after moving the focus.
          this._needBrowserFocusAtEnterKeyUp = false;

          // If the input field is still focused then a different window has
          // received focus, ignore the next focus event.
          this._ignoreFocus = document.activeElement == this._textbox;
        },
        true
      );

      this.addEventListener(
        "focus",
        event => {
          // Speculatively connect to the current engine's search URI (and
          // suggest URI, if different) to reduce request latency
          this.currentEngine.speculativeConnect({
            window,
            originAttributes: gBrowser.contentPrincipal.originAttributes,
          });

          if (this._ignoreFocus) {
            // This window has been re-focused, don't show the suggestions
            this._ignoreFocus = false;
            return;
          }

          // Don't open the suggestions if there is no text in the textbox.
          if (!this._textbox.value) {
            return;
          }

          // Don't open the suggestions if the mouse was used to focus the
          // textbox, that will be taken care of in the click handler.
          if (
            Services.focus.getLastFocusMethod(window) &
            Services.focus.FLAG_BYMOUSE
          ) {
            return;
          }

          this.openSuggestionsPanel();
        },
        true
      );

      this.addEventListener("mousedown", event => {
        this._preventClickSelectsAll = this._textbox.focused;
        // Ignore right clicks
        if (event.button != 0) {
          return;
        }

        // Ignore clicks on the search go button.
        if (event.originalTarget.classList.contains("search-go-button")) {
          return;
        }

        // Ignore clicks on menu items in the input's context menu.
        if (event.originalTarget.localName == "menuitem") {
          return;
        }

        let isIconClick = event.originalTarget.classList.contains(
          "searchbar-search-button"
        );

        // Hide popup when icon is clicked while popup is open
        if (isIconClick && this.textbox.popup.popupOpen) {
          this.textbox.popup.closePopup();
        } else if (isIconClick || this._textbox.value) {
          // Open the suggestions whenever clicking on the search icon or if there
          // is text in the textbox.
          this.openSuggestionsPanel(true);
        }
      });
    }

    _setupTextboxEventListeners() {
      this.textbox.addEventListener("input", event => {
        this.textbox.popup.removeAttribute("showonlysettings");
      });

      this.textbox.addEventListener("dragover", event => {
        let types = event.dataTransfer.types;
        if (
          types.includes("text/plain") ||
          types.includes("text/x-moz-text-internal")
        ) {
          event.preventDefault();
        }
      });

      this.textbox.addEventListener("drop", event => {
        let dataTransfer = event.dataTransfer;
        let data = dataTransfer.getData("text/plain");
        if (!data) {
          data = dataTransfer.getData("text/x-moz-text-internal");
        }
        if (data) {
          event.preventDefault();
          this.textbox.value = data;
          this.openSuggestionsPanel();
        }
      });

      this.textbox.addEventListener("contextmenu", event => {
        if (!this._menupopup) {
          this._buildContextMenu();
        }

        BrowserSearch.searchBar._textbox.closePopup();

        // Make sure the context menu isn't opened via keyboard shortcut. Check for text selection
        // before updating the state of any menu items.
        if (event.button) {
          this._maybeSelectAll();
        }

        // Update disabled state of menu items
        for (let item of this._menupopup.querySelectorAll("menuitem[cmd]")) {
          let command = item.getAttribute("cmd");
          let controller = document.commandDispatcher.getControllerForCommand(
            command
          );
          item.disabled = !controller.isCommandEnabled(command);
        }

        let pasteEnabled = document.commandDispatcher
          .getControllerForCommand("cmd_paste")
          .isCommandEnabled("cmd_paste");
        this._pasteAndSearchMenuItem.disabled = !pasteEnabled;

        this._menupopup.openPopupAtScreen(event.screenX, event.screenY, true);

        event.preventDefault();
      });
    }

    _initTextbox() {
      if (this.parentNode.parentNode.localName == "toolbarpaletteitem") {
        return;
      }

      this.setAttribute("role", "combobox");
      this.setAttribute("aria-owns", this.textbox.popup.id);

      // This overrides the searchParam property in autocomplete.xml. We're
      // hijacking this property as a vehicle for delivering the privacy
      // information about the window into the guts of nsSearchSuggestions.
      // Note that the setter is the same as the parent. We were not sure whether
      // we can override just the getter. If that proves to be the case, the setter
      // can be removed.
      Object.defineProperty(this.textbox, "searchParam", {
        get() {
          return (
            this.getAttribute("autocompletesearchparam") +
            (PrivateBrowsingUtils.isWindowPrivate(window) ? "|private" : "")
          );
        },
        set(val) {
          this.setAttribute("autocompletesearchparam", val);
        },
      });

      Object.defineProperty(this.textbox, "selectedButton", {
        get() {
          return this.popup.oneOffButtons.selectedButton;
        },
        set(val) {
          this.popup.oneOffButtons.selectedButton = val;
        },
      });

      // This is implemented so that when textbox.value is set directly (e.g.,
      // by tests), the one-off query is updated.
      this.textbox.onBeforeValueSet = aValue => {
        if (this.textbox.popup._oneOffButtons) {
          this.textbox.popup.oneOffButtons.query = aValue;
        }
        return aValue;
      };

      // Returns true if the event is handled by us, false otherwise.
      this.textbox.onBeforeHandleKeyDown = aEvent => {
        if (aEvent.getModifierState("Accel")) {
          if (
            aEvent.keyCode == KeyEvent.DOM_VK_DOWN ||
            aEvent.keyCode == KeyEvent.DOM_VK_UP
          ) {
            this.selectEngine(aEvent, aEvent.keyCode == KeyEvent.DOM_VK_DOWN);
            return true;
          }
          return false;
        }

        if (
          (AppConstants.platform == "macosx" &&
            aEvent.keyCode == KeyEvent.DOM_VK_F4) ||
          (aEvent.getModifierState("Alt") &&
            (aEvent.keyCode == KeyEvent.DOM_VK_DOWN ||
              aEvent.keyCode == KeyEvent.DOM_VK_UP))
        ) {
          if (!this.textbox.openSearch()) {
            aEvent.preventDefault();
            aEvent.stopPropagation();
            return true;
          }
        }

        let popup = this.textbox.popup;
        if (popup.popupOpen) {
          let suggestionsHidden =
            popup.richlistbox.getAttribute("collapsed") == "true";
          let numItems = suggestionsHidden ? 0 : popup.matchCount;
          return popup.oneOffButtons.handleKeyDown(aEvent, numItems, true);
        } else if (aEvent.keyCode == KeyEvent.DOM_VK_ESCAPE) {
          let undoCount = this.textbox.editor.transactionManager
            .numberOfUndoItems;
          if (undoCount) {
            this.textbox.editor.undo(undoCount);
          } else {
            this.textbox.select();
          }
          return true;
        }
        return false;
      };

      // This method overrides the autocomplete binding's openPopup (essentially
      // duplicating the logic from the autocomplete popup binding's
      // openAutocompletePopup method), modifying it so that the popup is aligned with
      // the inner textbox, but sized to not extend beyond the search bar border.
      this.textbox.openPopup = () => {
        // Entering customization mode after the search bar had focus causes
        // the popup to appear again, due to focus returning after the
        // hamburger panel closes. Don't open in that spurious event.
        if (document.documentElement.getAttribute("customizing") == "true") {
          return;
        }

        let popup = this.textbox.popup;
        if (!popup.mPopupOpen) {
          // Initially the panel used for the searchbar (PopupSearchAutoComplete
          // in browser.xhtml) is hidden to avoid impacting startup / new
          // window performance. The base binding's openPopup would normally
          // call the overriden openAutocompletePopup in
          // browser-search-autocomplete-result-popup binding to unhide the popup,
          // but since we're overriding openPopup we need to unhide the panel
          // ourselves.
          popup.hidden = false;

          // Don't roll up on mouse click in the anchor for the search UI.
          if (popup.id == "PopupSearchAutoComplete") {
            popup.setAttribute("norolluponanchor", "true");
          }

          popup.mInput = this.textbox;
          // clear any previous selection, see bugs 400671 and 488357
          popup.selectedIndex = -1;

          // Ensure the panel has a meaningful initial size and doesn't grow
          // unconditionally.
          requestAnimationFrame(() => {
            let { width } = window.windowUtils.getBoundsWithoutFlushing(this);
            if (popup.oneOffButtons) {
              // We have a min-width rule on search-panel-one-offs to show at
              // least 4 buttons, so take that into account here.
              width = Math.max(width, popup.oneOffButtons.buttonWidth * 4);
            }
            popup.style.setProperty("--panel-width", width + "px");
          });

          popup._invalidate();

          popup.openPopup(this, "after_start");
        }
      };

      this.textbox.openSearch = () => {
        if (!this.textbox.popupOpen) {
          this.openSuggestionsPanel();
          return false;
        }
        return true;
      };

      this.textbox.handleEnter = event => {
        // Toggle the open state of the add-engine menu button if it's
        // selected.  We're using handleEnter for this instead of listening
        // for the command event because a command event isn't fired.
        if (
          this.textbox.selectedButton &&
          this.textbox.selectedButton.getAttribute("anonid") ==
            "addengine-menu-button"
        ) {
          this.textbox.selectedButton.open = !this.textbox.selectedButton.open;
          return true;
        }
        // Otherwise, "call super": do what the autocomplete binding's
        // handleEnter implementation does.
        return this.textbox.mController.handleEnter(false, event || null);
      };

      // override |onTextEntered| in autocomplete.xml
      this.textbox.onTextEntered = event => {
        this.textbox.editor.transactionManager.clearUndoStack();

        let engine;
        let oneOff = this.textbox.selectedButton;
        if (oneOff) {
          if (!oneOff.engine) {
            oneOff.doCommand();
            return;
          }
          engine = oneOff.engine;
        }
        if (this.textbox.popupSelectedIndex != -1) {
          this.telemetrySelectedIndex = this.textbox.popupSelectedIndex;
          this.textbox.popupSelectedIndex = -1;
        }
        this.handleSearchCommand(event, engine);
      };

      this.textbox.onkeyup = event => {
        if (
          event.keyCode === KeyEvent.DOM_VK_RETURN &&
          this._needBrowserFocusAtEnterKeyUp
        ) {
          this._needBrowserFocusAtEnterKeyUp = false;
          gBrowser.selectedBrowser.focus();
        }
      };
    }

    _buildContextMenu() {
      const raw = `
        <menuitem data-l10n-id="text-action-undo" cmd="cmd_undo"/>
        <menuitem data-l10n-id="text-action-redo" cmd="cmd_redo"/>
        <menuseparator/>
        <menuitem data-l10n-id="text-action-cut" cmd="cmd_cut"/>
        <menuitem data-l10n-id="text-action-copy" cmd="cmd_copy"/>
        <menuitem data-l10n-id="text-action-paste" cmd="cmd_paste"/>
        <menuitem class="searchbar-paste-and-search"/>
        <menuitem data-l10n-id="text-action-delete" cmd="cmd_delete"/>
        <menuitem data-l10n-id="text-action-select-all" cmd="cmd_selectAll"/>
        <menuseparator/>
        <menuitem class="searchbar-clear-history"/>
      `;

      this._menupopup = this.querySelector(".textbox-contextmenu");

      let frag = MozXULElement.parseXULToFragment(raw);

      // Insert attributes that come from localized properties
      this._pasteAndSearchMenuItem = frag.querySelector(
        ".searchbar-paste-and-search"
      );
      this._pasteAndSearchMenuItem.setAttribute(
        "label",
        this._stringBundle.getString("cmd_pasteAndSearch")
      );

      let clearHistoryItem = frag.querySelector(".searchbar-clear-history");
      clearHistoryItem.setAttribute(
        "label",
        this._stringBundle.getString("cmd_clearHistory")
      );
      clearHistoryItem.setAttribute(
        "accesskey",
        this._stringBundle.getString("cmd_clearHistory_accesskey")
      );

      this._menupopup.appendChild(frag);

      this._menupopup.addEventListener("command", event => {
        switch (event.originalTarget) {
          case this._pasteAndSearchMenuItem:
            BrowserSearch.pasteAndSearch(event);
            break;
          case clearHistoryItem:
            let param = this.textbox.getAttribute("autocompletesearchparam");
            BrowserSearch.searchBar.FormHistory.update(
              { op: "remove", fieldname: param },
              null
            );
            this.textbox.value = "";
            break;
          default:
            let cmd = event.originalTarget.getAttribute("cmd");
            if (cmd) {
              let controller = document.commandDispatcher.getControllerForCommand(
                cmd
              );
              controller.doCommand(cmd);
            }
            break;
        }
      });
    }
  }

  customElements.define("searchbar", MozSearchbar);
}
