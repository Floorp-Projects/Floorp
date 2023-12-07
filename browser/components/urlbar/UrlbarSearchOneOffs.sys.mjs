/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { SearchOneOffs } from "resource:///modules/SearchOneOffs.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
  UrlbarProvidersManager: "resource:///modules/UrlbarProvidersManager.sys.mjs",
  UrlbarUtils: "resource:///modules/UrlbarUtils.sys.mjs",
});

/**
 * The one-off search buttons in the urlbar.
 */
export class UrlbarSearchOneOffs extends SearchOneOffs {
  /**
   * Constructor.
   *
   * @param {UrlbarView} view
   *   The parent UrlbarView.
   */
  constructor(view) {
    super(view.panel.querySelector(".search-one-offs"));
    this.view = view;
    this.input = view.input;
    lazy.UrlbarPrefs.addObserver(this);
    // Override the SearchOneOffs.jsm value for the Address Bar.
    this.disableOneOffsHorizontalKeyNavigation = true;
    this._webEngines = [];
    this.addEventListener("rebuild", this);
  }

  /**
   * Returns the local search mode one-off buttons.
   *
   * @returns {Array}
   *   The local one-off buttons.
   */
  get localButtons() {
    return this.getSelectableButtons(false).filter(b => b.source);
  }

  /**
   * Invoked when Web provided search engines list changes.
   *
   * @param {Array} engines Array of Web provided search engines. Each engine
   *        is defined as  { icon, name, tooltip, uri }.
   */
  updateWebEngines(engines) {
    this._webEngines = engines;
    this.invalidateCache();
    if (this.view.isOpen) {
      this._rebuild();
    }
  }

  /**
   * Enables (shows) or disables (hides) the one-offs.
   *
   * @param {boolean} enable
   *   True to enable, false to disable.
   */
  enable(enable) {
    if (enable) {
      this.telemetryOrigin = "urlbar";
      this.style.display = "";
      this.textbox = this.view.input.inputField;
      if (this.view.isOpen) {
        this._rebuild();
      }
      this.view.controller.addQueryListener(this);
    } else {
      this.telemetryOrigin = null;
      this.style.display = "none";
      this.textbox = null;
      this.view.controller.removeQueryListener(this);
    }
  }

  /**
   * Query listener method.  Delegates to the superclass.
   */
  onViewOpen() {
    this._on_popupshowing();
  }

  #queryContext;
  onQueryStarted(queryContext) {
    this.#queryContext = queryContext;
  }

  onQueryFinished(queryContext) {
    this.#buildQuickSuggestOptIn(queryContext);

    if (
      this.#quickSuggestOptInContainer &&
      !this.#quickSuggestOptInContainer.hidden
    ) {
      this.#quickSuggestOptInProvider._recordGlean("impression");
    }
  }

  #quickSuggestOptInContainer;
  get #quickSuggestOptInProvider() {
    return lazy.UrlbarProvidersManager.getProvider(
      "UrlbarProviderQuickSuggestContextualOptIn"
    );
  }

  #buildQuickSuggestOptIn(queryContext) {
    let provider = this.#quickSuggestOptInProvider;
    if (
      !provider._shouldDisplayContextualOptIn(queryContext) ||
      provider.isActive(queryContext)
    ) {
      if (this.#quickSuggestOptInContainer) {
        this.#quickSuggestOptInContainer.hidden = true;
      }
      return;
    }

    if (this.#quickSuggestOptInContainer) {
      this.#quickSuggestOptInContainer.hidden = false;
      this.#udpateQuickSuggestOptInCopy();
      return;
    }

    // The following is basically a copy of what UrlbarView generates for
    // ProviderQuickSuggestContextualOptIn's view template. Gross but good
    // enough for the experiment. Ultimately, if we decide to keep this UI at
    // the bottom, and when we replace the one-off buttons footer with a better
    // UI (e.g. search button), this can become a proper result again.
    let parser = new DOMParser();
    let doc = parser.parseFromString(
      `
<div xmlns="http://www.w3.org/1999/xhtml" class="urlbarView-quickSuggestContextualOptIn-one-off-container">
  <div class="urlbarView-row" role="presentation" type="dynamic">
    <span class="urlbarView-row-inner">
      <span class="urlbarView-dynamic-quickSuggestContextualOptIn-no-wrap urlbarView-no-wrap">
        <img class="urlbarView-dynamic-quickSuggestContextualOptIn-icon urlbarView-favicon" src="chrome://branding/content/icon32.png" />
        <span class="urlbarView-dynamic-quickSuggestContextualOptIn-text-container">
          <strong class="urlbarView-dynamic-quickSuggestContextualOptIn-title"></strong>
          <span class="urlbarView-dynamic-quickSuggestContextualOptIn-description">
            <a class="urlbarView-dynamic-quickSuggestContextualOptIn-learn_more" data-l10n-name="learn-more-link" selectable="" name="learn_more" id="urlbarView-footer-quickSuggestContextualOptIn-learn_more"></a>
          </span>
        </span>
      </span>
    </span>
    <span primary="" name="allow" class="urlbarView-button urlbarView-button-0" role="button" data-l10n-id="urlbar-firefox-suggest-contextual-opt-in-allow" id="urlbarView-footer-quickSuggestContextualOptIn-allow"></span>
    <span name="dismiss" class="urlbarView-button urlbarView-button-1" role="button" data-l10n-id="urlbar-firefox-suggest-contextual-opt-in-dismiss" id="urlbarView-footer-quickSuggestContextualOptIn-dismiss"></span>
  </div>
</div>
      `,
      "text/html"
    );
    this.#quickSuggestOptInContainer = this.document.importNode(
      doc.body.firstElementChild,
      true
    );

    // DOMParser normalizes attribute names to lowercase, so need to set this one after the fact.
    this.#quickSuggestOptInContainer.firstElementChild.setAttribute(
      "dynamicType",
      "quickSuggestContextualOptIn"
    );

    this.container.appendChild(this.#quickSuggestOptInContainer);
    this.#quickSuggestOptInContainer.addEventListener("keydown", this);
    this.#udpateQuickSuggestOptInCopy();
  }

  #udpateQuickSuggestOptInCopy() {
    let alternativeCopy = lazy.UrlbarPrefs.get(
      "quicksuggest.contextualOptIn.sayHello"
    );
    this.document.l10n.setAttributes(
      this.#quickSuggestOptInContainer.querySelector(
        ".urlbarView-dynamic-quickSuggestContextualOptIn-title"
      ),
      alternativeCopy
        ? "urlbar-firefox-suggest-contextual-opt-in-title-2"
        : "urlbar-firefox-suggest-contextual-opt-in-title-1"
    );
    this.document.l10n.setAttributes(
      this.#quickSuggestOptInContainer.querySelector(
        ".urlbarView-dynamic-quickSuggestContextualOptIn-description"
      ),
      alternativeCopy
        ? "urlbar-firefox-suggest-contextual-opt-in-description-2"
        : "urlbar-firefox-suggest-contextual-opt-in-description-1"
    );
  }

  #isQuickSuggestOptInElement(element) {
    return (
      this.#quickSuggestOptInContainer &&
      element?.compareDocumentPosition(this.#quickSuggestOptInContainer) &
        Node.DOCUMENT_POSITION_CONTAINS
    );
  }

  #handleQuickSuggestOptInCommand(element) {
    if (this.#isQuickSuggestOptInElement(element)) {
      this.#quickSuggestOptInProvider._handleCommand(
        element,
        this.view.controller,
        null,
        this.#quickSuggestOptInContainer
      );
      return true;
    }
    return false;
  }

  /**
   * Query listener method.  Delegates to the superclass.
   */
  onViewClose() {
    this._on_popuphidden();
  }

  /**
   * @returns {boolean}
   *   True if the one-offs are connected to a view.
   */
  get hasView() {
    // Return true if the one-offs are enabled.  We set style.display = "none"
    // when they're disabled, and we hide the container when there are no
    // engines to show.
    return this.style.display != "none" && !this.container.hidden;
  }

  /**
   * @returns {boolean}
   *   True if the view is open.
   */
  get isViewOpen() {
    return this.view.isOpen;
  }

  /**
   * The selected one-off including the search-settings button.
   *
   * @param {DOMElement|null} button
   *   The selected one-off button. Null if no one-off is selected.
   */
  set selectedButton(button) {
    if (this.selectedButton == button) {
      return;
    }

    if (this.#isQuickSuggestOptInElement(button)) {
      this.#quickSuggestOptInProvider.onBeforeSelection(null, button);
    }

    super.selectedButton = button;

    let expectedSearchMode;
    if (button && button != this.view.oneOffSearchButtons.settingsButton) {
      expectedSearchMode = {
        engineName: button.engine?.name,
        source: button.source,
        entry: "oneoff",
      };
      this.input.searchMode = expectedSearchMode;
    } else if (this.input.searchMode) {
      // Restore the previous state. We do this only if we're in search mode, as
      // an optimization in the common case of cycling through normal results.
      this.input.restoreSearchModeState();
    }
  }

  get selectedButton() {
    return super.selectedButton;
  }

  getSelectableButtons(aIncludeNonEngineButtons) {
    const buttons = super.getSelectableButtons(aIncludeNonEngineButtons);

    if (
      aIncludeNonEngineButtons &&
      this.#quickSuggestOptInContainer &&
      !this.#quickSuggestOptInContainer.hidden
    ) {
      buttons.push(
        ...this.#quickSuggestOptInContainer.querySelectorAll(
          "[role=button], [selectable]"
        )
      );
    }

    return buttons;
  }

  /**
   * The selected index in the view or -1 if there is no selection.
   *
   * @returns {number}
   */
  get selectedViewIndex() {
    return this.view.selectedRowIndex;
  }
  set selectedViewIndex(val) {
    this.view.selectedRowIndex = val;
  }

  /**
   * Closes the view.
   */
  closeView() {
    if (this.view) {
      this.view.close();
    }
  }

  /**
   * Called when a one-off is clicked.
   *
   * @param {event} event
   *   The event that triggered the pick.
   * @param {object} searchMode
   *   Used by UrlbarInput.setSearchMode to enter search mode. See setSearchMode
   *   documentation for details.
   */
  handleSearchCommand(event, searchMode) {
    // The settings button and adding engines are a special case and executed
    // immediately.
    if (
      this.selectedButton == this.view.oneOffSearchButtons.settingsButton ||
      this.selectedButton.classList.contains(
        "searchbar-engine-one-off-add-engine"
      )
    ) {
      this.input.controller.engagementEvent.discard();
      this.selectedButton.doCommand();
      this.selectedButton = null;
      return;
    }

    if (this.#handleQuickSuggestOptInCommand(this.selectedButton)) {
      this.input.controller.engagementEvent.discard();
      this.selectedButton = null;
      return;
    }

    // We allow autofill in local but not remote search modes.
    let startQueryParams = {
      allowAutofill:
        !searchMode.engineName &&
        searchMode.source != lazy.UrlbarUtils.RESULT_SOURCE.SEARCH,
      event,
    };

    let userTypedSearchString =
      this.input.value && this.input.getAttribute("pageproxystate") != "valid";
    let engine = Services.search.getEngineByName(searchMode.engineName);

    let { where, params } = this._whereToOpen(event);

    // Some key combinations should execute a search immediately. We handle
    // these here, outside the switch statement.
    if (
      userTypedSearchString &&
      engine &&
      (event.shiftKey || where != "current")
    ) {
      this.input.handleNavigation({
        event,
        oneOffParams: {
          openWhere: where,
          openParams: params,
          engine: this.selectedButton.engine,
        },
      });
      this.selectedButton = null;
      return;
    }

    // Handle opening search mode in either the current tab or in a new tab.
    switch (where) {
      case "current": {
        this.input.searchMode = searchMode;
        this.input.startQuery(startQueryParams);
        break;
      }
      case "tab": {
        // We set this.selectedButton when switching tabs. If we entered search
        // mode preview here, it could be cleared when this.selectedButton calls
        // setSearchMode.
        searchMode.isPreview = false;

        let newTab = this.input.window.gBrowser.addTrustedTab("about:newtab");
        this.input.setSearchMode(searchMode, newTab.linkedBrowser);
        if (userTypedSearchString) {
          // Set the search string for the new tab.
          newTab.linkedBrowser.userTypedValue = this.input.value;
        }
        if (!params?.inBackground) {
          this.input.window.gBrowser.selectedTab = newTab;
          newTab.ownerGlobal.gURLBar.startQuery(startQueryParams);
        }
        break;
      }
      default: {
        this.input.searchMode = searchMode;
        this.input.startQuery(startQueryParams);
        this.input.select();
        break;
      }
    }

    this.selectedButton = null;
  }

  /**
   * Sets the tooltip for a one-off button with an engine.  This should set
   * either the `tooltiptext` attribute or the relevant l10n ID.
   *
   * @param {element} button
   *   The one-off button.
   */
  setTooltipForEngineButton(button) {
    let aliases = button.engine.aliases;
    if (!aliases.length) {
      super.setTooltipForEngineButton(button);
      return;
    }
    this.document.l10n.setAttributes(
      button,
      "search-one-offs-engine-with-alias",
      {
        engineName: button.engine.name,
        alias: aliases[0],
      }
    );
  }

  /**
   * Overrides the willHide method in the superclass to account for the local
   * search mode buttons.
   *
   * @returns {boolean}
   *   True if we will hide the one-offs when they are requested.
   */
  async willHide() {
    // We need to call super.willHide() even when we return false below because
    // it has the necessary side effect of creating this._engineInfo.
    let superWillHide = await super.willHide();
    if (
      lazy.UrlbarUtils.LOCAL_SEARCH_MODES.some(m =>
        lazy.UrlbarPrefs.get(m.pref)
      )
    ) {
      return false;
    }
    return superWillHide;
  }

  /**
   * Called when a pref tracked by UrlbarPrefs changes.
   *
   * @param {string} changedPref
   *   The name of the pref, relative to `browser.urlbar.` if the pref is in
   *   that branch.
   */
  onPrefChanged(changedPref) {
    // Invalidate the engine cache when the local-one-offs-related prefs change
    // so that the one-offs rebuild themselves the next time the view opens.
    if (
      [...lazy.UrlbarUtils.LOCAL_SEARCH_MODES.map(m => m.pref)].includes(
        changedPref
      )
    ) {
      this.invalidateCache();
    }
  }

  /**
   * Overrides _getAddEngines to return engines that can be added.
   *
   * @returns {Array} engines
   */
  _getAddEngines() {
    return this._webEngines;
  }

  /**
   * Overrides _rebuildEngineList to add the local one-offs.
   *
   * @param {Array} engines
   *    The search engines to add.
   * @param {Array} addEngines
   *        The engines that can be added.
   */
  _rebuildEngineList(engines, addEngines) {
    super._rebuildEngineList(engines, addEngines);

    for (let { source, pref, restrict } of lazy.UrlbarUtils
      .LOCAL_SEARCH_MODES) {
      if (!lazy.UrlbarPrefs.get(pref)) {
        continue;
      }
      let name = lazy.UrlbarUtils.getResultSourceName(source);
      let button = this.document.createXULElement("button");
      button.id = `urlbar-engine-one-off-item-${name}`;
      button.setAttribute("class", "searchbar-engine-one-off-item");
      button.setAttribute("tabindex", "-1");
      this.document.l10n.setAttributes(button, `search-one-offs-${name}`, {
        restrict,
      });
      button.source = source;
      this.buttons.appendChild(button);
    }
  }

  /**
   * Overrides the superclass's click listener to handle clicks on local
   * one-offs in addition to engine one-offs.
   *
   * @param {event} event
   *   The click event.
   */
  _on_click(event) {
    // Ignore right clicks.
    if (event.button == 2) {
      return;
    }

    let button = event.originalTarget;

    if (this.#handleQuickSuggestOptInCommand(button)) {
      return;
    }

    if (!button.engine && !button.source) {
      return;
    }

    this.selectedButton = button;
    this.handleSearchCommand(event, {
      engineName: button.engine?.name,
      source: button.source,
      entry: "oneoff",
    });
  }

  /**
   * Overrides the superclass's contextmenu listener to handle the context menu.
   *
   * @param {event} event
   *   The contextmenu event.
   */
  _on_contextmenu(event) {
    // Prevent the context menu from appearing.
    event.preventDefault();
  }

  _on_rebuild() {
    if (this.#queryContext) {
      this.#buildQuickSuggestOptIn(this.#queryContext);
    }
  }
}
