/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["SearchOneOffs"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
XPCOMUtils.defineLazyModuleGetters(this, {
  clearTimeout: "resource://gre/modules/Timer.jsm",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.jsm",
  SearchUIUtils: "resource:///modules/SearchUIUtils.jsm",
  Services: "resource://gre/modules/Services.jsm",
  setTimeout: "resource://gre/modules/Timer.jsm",
});

/**
 * Defines the search one-off button elements. These are displayed at the bottom
 * of the address bar and search bar. The address bar buttons are a subclass in
 * browser/components/urlbar/UrlbarSearchOneOffs.jsm. If you are adding a new
 * subclass, see "Methods for subclasses to override" below.
 */
class SearchOneOffs {
  constructor(container) {
    this.container = container;
    this.window = container.ownerGlobal;
    this.document = container.ownerDocument;

    this.container.appendChild(
      this.window.MozXULElement.parseXULToFragment(
        `
      <hbox class="search-panel-one-offs-header search-panel-header">
        <label class="search-panel-one-offs-header-label" data-l10n-id="search-one-offs-with-title"/>
      </hbox>
      <box class="search-panel-one-offs-container">
        <hbox class="search-panel-one-offs" role="group"/>
        <hbox class="search-one-offs-spacer"/>
        <button class="searchbar-engine-one-off-item search-setting-button-compact" tabindex="-1" data-l10n-id="search-one-offs-change-settings-compact-button"/>
      </box>
      <vbox class="search-add-engines"/>
      <menuseparator class="searchbar-separator"/>
      <button class="search-setting-button" pack="start" data-l10n-id="search-one-offs-change-settings-button"/>
      <box>
        <menupopup class="search-one-offs-context-menu">
          <menuitem class="search-one-offs-context-open-in-new-tab" data-l10n-id="search-one-offs-context-open-new-tab"/>
          <menuitem class="search-one-offs-context-set-default" data-l10n-id="search-one-offs-context-set-as-default"/>
          <menuitem class="search-one-offs-context-set-default-private" data-l10n-id="search-one-offs-context-set-as-default-private"/>
        </menupopup>
      </box>
      `
      )
    );

    this._popup = null;
    this._textbox = null;

    this._textboxWidth = 0;

    /**
     * Set this to a string that identifies your one-offs consumer.  It'll
     * be appended to telemetry recorded with maybeRecordTelemetry().
     */
    this.telemetryOrigin = "";

    this._query = "";

    this._selectedButton = null;

    this.buttons = this.querySelector(".search-panel-one-offs");

    this.header = this.querySelector(".search-panel-one-offs-header");

    this.addEngines = this.querySelector(".search-add-engines");

    this.settingsButton = this.querySelector(".search-setting-button");

    this.settingsButtonCompact = this.querySelector(
      ".search-setting-button-compact"
    );

    this.spacerCompact = this.querySelector(".search-one-offs-spacer");

    this.contextMenuPopup = this.querySelector(".search-one-offs-context-menu");

    this._bundle = null;

    /**
     * When a context menu is opened on a one-off button, this is set to the
     * engine of that button for use with the context menu actions.
     */
    this._contextEngine = null;

    this._engineInfo = null;

    /**
     * `_rebuild()` is async, because it queries the Search Service, which means
     * there is a potential for a race when it's called multiple times in succession.
     */
    this._rebuilding = false;

    /**
     * If a page offers more than this number of engines, the add-engines
     * menu button is shown, instead of showing the engines directly in the
     * popup.
     */
    this._addEngineMenuThreshold = 5;

    /**
     * All this stuff is to make the add-engines menu button behave like an
     * actual menu.  The add-engines menu button is shown when there are
     * many engines offered by the current site.
     */
    this._addEngineMenuTimeoutMs = 200;

    this._addEngineMenuTimeout = null;

    this._addEngineMenuShouldBeOpen = false;

    this.addEventListener("mousedown", this);
    this.addEventListener("mousemove", this);
    this.addEventListener("mouseout", this);
    this.addEventListener("click", this);
    this.addEventListener("command", this);
    this.addEventListener("contextmenu", this);

    // Prevent popup events from the context menu from reaching the autocomplete
    // binding (or other listeners).
    let listener = aEvent => aEvent.stopPropagation();
    this.contextMenuPopup.addEventListener("popupshowing", listener);
    this.contextMenuPopup.addEventListener("popuphiding", listener);
    this.contextMenuPopup.addEventListener("popupshown", aEvent => {
      aEvent.stopPropagation();
    });
    this.contextMenuPopup.addEventListener("popuphidden", aEvent => {
      aEvent.stopPropagation();
    });

    // Add weak referenced observers to invalidate our cached list of engines.
    this.QueryInterface = ChromeUtils.generateQI([
      "nsIObserver",
      "nsISupportsWeakReference",
    ]);
    Services.prefs.addObserver("browser.search.hiddenOneOffs", this, true);
    Services.obs.addObserver(this, "browser-search-engine-modified", true);
    Services.obs.addObserver(this, "browser-search-service", true);

    // Rebuild the buttons when the theme changes.  See bug 1357800 for
    // details.  Summary: On Linux, switching between themes can cause a row
    // of buttons to disappear.
    Services.obs.addObserver(this, "lightweight-theme-changed", true);

    // This defaults to false in the Search Bar, subclasses can change their
    // default in the constructor.
    this.disableOneOffsHorizontalKeyNavigation = false;
  }

  addEventListener(...args) {
    this.container.addEventListener(...args);
  }

  removeEventListener(...args) {
    this.container.removeEventListener(...args);
  }

  dispatchEvent(...args) {
    this.container.dispatchEvent(...args);
  }

  getAttribute(...args) {
    return this.container.getAttribute(...args);
  }

  setAttribute(...args) {
    this.container.setAttribute(...args);
  }

  querySelector(...args) {
    return this.container.querySelector(...args);
  }

  handleEvent(event) {
    let methodName = "_on_" + event.type;
    if (methodName in this) {
      this[methodName](event);
    } else {
      throw new Error("Unrecognized search-one-offs event: " + event.type);
    }
  }

  /**
   * @returns {boolean}
   *   True if we will hide the one-offs when they are requested.
   */
  async willHide() {
    if (this._engineInfo?.willHide !== undefined) {
      return this._engineInfo.willHide;
    }
    let engineInfo = await this.getEngineInfo();
    let oneOffCount = engineInfo.engines.length;
    this._engineInfo.willHide =
      !oneOffCount ||
      (oneOffCount == 1 &&
        engineInfo.engines[0].name == engineInfo.default.name);
    return this._engineInfo.willHide;
  }

  /**
   * Invalidates the engine cache. After invalidating the cache, the one-offs
   * will be rebuilt the next time they are shown.
   */
  invalidateCache() {
    if (!this._rebuilding) {
      this._engineInfo = null;
    }
  }

  /**
   * Width in pixels of the one-off buttons.
   */
  get buttonWidth() {
    return 48;
  }

  /**
   * Height in pixels of the one-off buttons.
   */
  get buttonHeight() {
    return 32;
  }

  /**
   * The popup that contains the one-offs.
   *
   * @param {DOMElement} val
   *        The new value to set.
   */
  set popup(val) {
    if (this._popup) {
      this._popup.removeEventListener("popupshowing", this);
      this._popup.removeEventListener("popuphidden", this);
    }
    if (val) {
      val.addEventListener("popupshowing", this);
      val.addEventListener("popuphidden", this);
    }
    this._popup = val;

    // If the popup is already open, rebuild the one-offs now.  The
    // popup may be opening, so check that the state is not closed
    // instead of checking popupOpen.
    if (val && val.state != "closed") {
      this._rebuild();
    }
  }

  get popup() {
    return this._popup;
  }

  /**
   * The textbox associated with the one-offs.  Set this to a textbox to
   * automatically keep the related one-offs UI up to date.  Otherwise you
   * can leave it null/undefined, and in that case you should update the
   * query property manually.
   *
   * @param {DOMElement} val
   *        The new value to set.
   */
  set textbox(val) {
    if (this._textbox) {
      this._textbox.removeEventListener("input", this);
    }
    if (val) {
      val.addEventListener("input", this);
    }
    this._textbox = val;
  }

  get style() {
    return this.container.style;
  }

  get textbox() {
    return this._textbox;
  }

  /**
   * The query string currently shown in the one-offs.  If the textbox
   * property is non-null, then this is automatically updated on
   * input.
   *
   * @param {string} val
   *        The new query string to set.
   */
  set query(val) {
    this._query = val;
    if (this.isViewOpen) {
      let isOneOffSelected =
        this.selectedButton &&
        this.selectedButton.classList.contains("searchbar-engine-one-off-item");
      // Typing de-selects the settings or opensearch buttons at the bottom
      // of the search panel, as typing shows the user intends to search.
      if (this.selectedButton && !isOneOffSelected) {
        this.selectedButton = null;
      }
    }
  }

  get query() {
    return this._query;
  }

  /**
   * The selected one-off, a xul:button, including the add-engine button
   * and the search-settings button.
   *
   * @param {DOMElement|null} val
   *        The selected one-off button. Null if no one-off is selected.
   */
  set selectedButton(val) {
    let previousButton = this._selectedButton;
    if (previousButton) {
      previousButton.removeAttribute("selected");
    }
    if (val) {
      val.setAttribute("selected", "true");
    }
    this._selectedButton = val;

    if (this.textbox) {
      if (val) {
        this.textbox.setAttribute("aria-activedescendant", val.id);
      } else {
        let active = this.textbox.getAttribute("aria-activedescendant");
        if (active && active.includes("-engine-one-off-item-")) {
          this.textbox.removeAttribute("aria-activedescendant");
        }
      }
    }

    let event = new CustomEvent("SelectedOneOffButtonChanged", {
      previousSelectedButton: previousButton,
    });
    this.dispatchEvent(event);
  }

  get selectedButton() {
    return this._selectedButton;
  }

  /**
   * The index of the selected one-off, including the add-engine button
   * and the search-settings button.
   *
   * @param {number} val
   *        The new index to set, -1 for nothing selected.
   */
  set selectedButtonIndex(val) {
    let buttons = this.getSelectableButtons(true);
    this.selectedButton = buttons[val];
  }

  get selectedButtonIndex() {
    let buttons = this.getSelectableButtons(true);
    for (let i = 0; i < buttons.length; i++) {
      if (buttons[i] == this._selectedButton) {
        return i;
      }
    }
    return -1;
  }

  get compact() {
    return this.getAttribute("compact") == "true";
  }

  get bundle() {
    if (!this._bundle) {
      const kBundleURI = "chrome://browser/locale/search.properties";
      this._bundle = Services.strings.createBundle(kBundleURI);
    }
    return this._bundle;
  }

  async getEngineInfo() {
    if (this._engineInfo) {
      return this._engineInfo;
    }

    this._engineInfo = {};
    if (PrivateBrowsingUtils.isWindowPrivate(this.window)) {
      this._engineInfo.default = await Services.search.getDefaultPrivate();
    } else {
      this._engineInfo.default = await Services.search.getDefault();
    }

    let currentEngineNameToIgnore;
    if (!this.getAttribute("includecurrentengine")) {
      currentEngineNameToIgnore = this._engineInfo.default.name;
    }

    let pref = Services.prefs.getStringPref("browser.search.hiddenOneOffs");
    let hiddenList = pref ? pref.split(",") : [];

    this._engineInfo.engines = (
      await Services.search.getVisibleEngines()
    ).filter(e => {
      let name = e.name;
      return (
        (!currentEngineNameToIgnore || name != currentEngineNameToIgnore) &&
        !hiddenList.includes(name)
      );
    });

    return this._engineInfo;
  }

  observe(aEngine, aTopic, aData) {
    // Make sure the engine list was updated.
    this.invalidateCache();
  }

  /**
   * Infallible, non-re-entrant version of `__rebuild()`.
   */
  async _rebuild() {
    if (this._rebuilding) {
      return;
    }

    this._rebuilding = true;
    try {
      await this.__rebuild();
    } catch (ex) {
      Cu.reportError("Search-one-offs::_rebuild() error: " + ex);
    } finally {
      this._rebuilding = false;
    }
  }

  /**
   * Builds all the UI.
   */
  async __rebuild() {
    // Handle opensearch items. This needs to be done before building the
    // list of one off providers, as that code will return early if all the
    // alternative engines are hidden.
    // Skip this in compact mode, ie. for the urlbar.
    if (!this.compact) {
      this._rebuildAddEngineList();
    }

    // Return early if the list of engines has not changed.
    if (!this.popup && this._engineInfo?.domWasUpdated) {
      return;
    }

    // Return early if the engines and panel width have not changed.
    if (this.popup && this._textbox) {
      let textboxWidth = await this.window.promiseDocumentFlushed(() => {
        return this._textbox.clientWidth;
      });
      if (
        this._engineInfo?.domWasUpdated &&
        this._textboxWidth == textboxWidth
      ) {
        return;
      }
      this._textboxWidth = textboxWidth;
    }

    // Finally, build the list of one-off buttons.
    while (this.buttons.firstElementChild) {
      this.buttons.firstElementChild.remove();
    }

    let headerText = this.header.querySelector(
      ".search-panel-one-offs-header-label"
    );
    headerText.id = this.telemetryOrigin + "-one-offs-header-label";
    this.buttons.setAttribute("aria-labelledby", headerText.id);

    let hideOneOffs = await this.willHide();

    // The _engineInfo cache is used by more consumers, thus it is not a good
    // representation of whether this method already updated the one-off buttons
    // DOM. For this reason we introduce a separate flag tracking the DOM
    // updating, and use it to know when it's okay to not rebuild the one-offs.
    // We set this early, since we might either rebuild the DOM or hide it.
    this._engineInfo.domWasUpdated = true;

    if (this.compact) {
      this.container.hidden = hideOneOffs;
    } else {
      // Hide everything except the settings button.
      this.header.hidden = this.buttons.hidden = hideOneOffs;
    }

    if (hideOneOffs) {
      return;
    }

    if (this.compact) {
      this.spacerCompact.setAttribute("flex", "1");
    }

    let engines = (await this.getEngineInfo()).engines;
    if (this.popup) {
      let buttonsWidth = this.popup.clientWidth;

      // There's one weird thing to guard against: when layout pixels
      // aren't an integral multiple of device pixels, the last button
      // of each row sometimes gets pushed to the next row, depending on the
      // panel and button widths.
      // This is likely because the clientWidth getter rounds the value, but
      // the panel's border width is not an integer.
      // As a workaround, decrement the width if the scale is not an integer.
      let scale = this.window.windowUtils.screenPixelsPerCSSPixel;
      if (Math.floor(scale) != scale) {
        --buttonsWidth;
      }

      // If the header string is very long, then the searchbar buttons will
      // overflow their container unless max-width is set.
      this.buttons.style.setProperty("max-width", `${buttonsWidth}px`);

      // In very narrow windows, we should always have at least one button
      // per row.
      buttonsWidth = Math.max(buttonsWidth, this.buttonWidth);

      let enginesPerRow = Math.floor(buttonsWidth / this.buttonWidth);
      // There will be an empty area of:
      //   buttonsWidth - enginesPerRow * this.buttonWidth  px
      // at the end of each row.

      // If the <div> with the list of search engines doesn't have
      // a fixed height, the panel will be sized incorrectly, causing the bottom
      // of the suggestion <tree> to be hidden.
      let rowCount = Math.ceil(engines.length / enginesPerRow);
      let height = rowCount * this.buttonHeight;
      this.buttons.style.setProperty("height", `${height}px`);
    }
    // Ensure we can refer to the settings buttons by ID:
    let origin = this.telemetryOrigin;
    this.settingsButton.id = origin + "-anon-search-settings";
    this.settingsButtonCompact.id = origin + "-anon-search-settings-compact";

    this._rebuildEngineList(engines);
    this.dispatchEvent(new Event("rebuild"));
  }

  /**
   * Adds one-offs for the given engines to the DOM.
   *
   * @param {array} engines
   *        The engines to add.
   */
  _rebuildEngineList(engines) {
    for (let i = 0; i < engines.length; ++i) {
      let engine = engines[i];
      let button = this.document.createXULElement("button");
      button.engine = engine;
      button.id = this._buttonIDForEngine(engine);
      let iconURI =
        engine.iconURI?.spec ||
        "chrome://browser/skin/search-engine-placeholder.png";
      button.setAttribute("image", iconURI);
      button.setAttribute("class", "searchbar-engine-one-off-item");
      button.setAttribute("tabindex", "-1");
      this.setTooltipForEngineButton(button);
      this.buttons.appendChild(button);
    }
  }

  _rebuildAddEngineList() {
    let list = this.addEngines;
    while (list.firstChild) {
      list.firstChild.remove();
    }

    // Add a button for each engine that the page in the selected browser
    // offers, except when there are too many offered engines.
    // The popup isn't designed to handle too many (by scrolling for
    // example), so a page could break the popup by offering too many.
    // Instead, add a single menu button with a submenu of all the engines.

    if (!this.window.gBrowser.selectedBrowser.engines) {
      return;
    }

    let engines = this.window.gBrowser.selectedBrowser.engines;
    let tooManyEngines = engines.length > this._addEngineMenuThreshold;

    if (tooManyEngines) {
      // Make the top-level menu button.
      let button = this.document.createXULElement("toolbarbutton");
      button.classList.add("addengine-menu-button", "addengine-item");
      button.setAttribute("badged", "true");
      button.setAttribute("type", "menu");
      button.setAttribute("wantdropmarker", "true");
      button.setAttribute(
        "label",
        this.bundle.GetStringFromName("cmd_addFoundEngineMenu")
      );
      button.setAttribute("crop", "end");
      button.setAttribute("pack", "start");

      // Set the menu button's image to the image of the first engine.  The
      // offered engines may have differing images, so there's no perfect
      // choice here.
      let engine = engines[0];
      if (engine.icon) {
        button.setAttribute("image", engine.icon);
      }
      list.appendChild(button);

      // Now make the button's child menupopup.
      list = this.document.createXULElement("menupopup");
      button.appendChild(list);
      list.setAttribute("class", "addengine-menu");
      list.setAttribute("position", "topright topleft");

      // Events from child menupopups bubble up to the autocomplete binding,
      // which breaks it, so prevent these events from propagating.
      let suppressEventTypes = [
        "popupshowing",
        "popuphiding",
        "popupshown",
        "popuphidden",
      ];
      for (let type of suppressEventTypes) {
        list.addEventListener(type, event => {
          event.stopPropagation();
        });
      }
    }

    // Finally, add the engines to the list.  If there aren't too many
    // engines, the list is the search-add-engines vbox.  Otherwise it's the
    // menupopup created earlier.  In the latter case, create menuitem
    // elements instead of buttons, because buttons don't get keyboard
    // handling for free inside menupopups.
    let eltType = tooManyEngines ? "menuitem" : "toolbarbutton";
    for (let engine of engines) {
      let button = this.document.createXULElement(eltType);
      button.classList.add("addengine-item");
      if (!tooManyEngines) {
        button.setAttribute("badged", "true");
      }
      button.id =
        this.telemetryOrigin +
        "-add-engine-" +
        this._fixUpEngineNameForID(engine.title);
      let label = this.bundle.formatStringFromName("cmd_addFoundEngine", [
        engine.title,
      ]);
      button.setAttribute("label", label);
      button.setAttribute("crop", "end");
      button.setAttribute("tooltiptext", engine.title + "\n" + engine.uri);
      button.setAttribute("uri", engine.uri);
      button.setAttribute("title", engine.title);
      if (engine.icon) {
        button.setAttribute("image", engine.icon);
      }
      if (tooManyEngines) {
        button.classList.add("menuitem-iconic");
      } else {
        button.setAttribute("pack", "start");
      }
      list.appendChild(button);
    }
  }

  _buttonIDForEngine(engine) {
    return (
      this.telemetryOrigin +
      "-engine-one-off-item-" +
      this._fixUpEngineNameForID(engine.name)
    );
  }

  _fixUpEngineNameForID(name) {
    return name.replace(/ /g, "-");
  }

  _buttonForEngine(engine) {
    let id = this._buttonIDForEngine(engine);
    return this.document.getElementById(id);
  }

  getSelectableButtons(aIncludeNonEngineButtons) {
    let buttons = [];
    for (
      let oneOff = this.buttons.firstElementChild;
      oneOff;
      oneOff = oneOff.nextElementSibling
    ) {
      buttons.push(oneOff);
    }

    if (aIncludeNonEngineButtons) {
      for (
        let addEngine = this.addEngines.firstElementChild;
        addEngine;
        addEngine = addEngine.nextElementSibling
      ) {
        buttons.push(addEngine);
      }
      buttons.push(
        this.compact ? this.settingsButtonCompact : this.settingsButton
      );
    }

    return buttons;
  }

  /**
   * Returns information on where a search results page should be loaded: in the
   * current tab or a new tab.
   *
   * @param {event} aEvent
   *        The event that triggered the page load.
   * @param {boolean} [aForceNewTab]
   *        True to force the load in a new tab.
   * @returns {object} An object { where, params }.  `where` is a string:
   *          "current" or "tab".  `params` is an object further describing how
   *          the page should be loaded.
   */
  _whereToOpen(aEvent, aForceNewTab = false) {
    let where = "current";
    let params;
    // Open ctrl/cmd clicks on one-off buttons in a new background tab.
    if (aForceNewTab) {
      where = "tab";
      if (Services.prefs.getBoolPref("browser.tabs.loadInBackground")) {
        params = {
          inBackground: true,
        };
      }
    } else {
      let newTabPref = Services.prefs.getBoolPref("browser.search.openintab");
      if (
        (aEvent instanceof KeyboardEvent && aEvent.altKey) ^ newTabPref &&
        !this.window.gBrowser.selectedTab.isEmpty
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

    return { where, params };
  }

  /**
   * Increments or decrements the index of the currently selected one-off.
   *
   * @param {boolean} aForward
   *        If true, the index is incremented, and if false, the index is
   *        decremented.
   * @param {boolean} aIncludeNonEngineButtons
   *        If true, buttons that do not have engines are included.
   *        These buttons include the OpenSearch and settings buttons.  For
   *        example, if the currently selected button is an engine button,
   *        the next button is the settings button, and you pass true for
   *        aForward, then passing true for this value would cause the
   *        settings to be selected.  Passing false for this value would
   *        cause the selection to clear or wrap around, depending on what
   *        value you passed for the aWrapAround parameter.
   * @param {boolean} aWrapAround
   *        If true, the selection wraps around between the first and last
   *        buttons.
   */
  advanceSelection(aForward, aIncludeNonEngineButtons, aWrapAround) {
    let buttons = this.getSelectableButtons(aIncludeNonEngineButtons);
    let index;
    if (this.selectedButton) {
      let inc = aForward ? 1 : -1;
      let oldIndex = buttons.indexOf(this.selectedButton);
      index = (oldIndex + inc + buttons.length) % buttons.length;
      if (
        !aWrapAround &&
        ((aForward && index <= oldIndex) || (!aForward && oldIndex <= index))
      ) {
        // The index has wrapped around, but wrapping around isn't
        // allowed.
        index = -1;
      }
    } else {
      index = aForward ? 0 : buttons.length - 1;
    }
    this.selectedButton = index < 0 ? null : buttons[index];
  }

  /**
   * This handles key presses specific to the one-off buttons like Tab and
   * Alt+Up/Down, and Up/Down keys within the buttons.  Since one-off buttons
   * are always used in conjunction with a list of some sort (in this.popup),
   * it also handles Up/Down keys that cross the boundaries between list
   * items and the one-off buttons.
   *
   * If this method handles the key press, then it will call
   * event.preventDefault() and return true.
   *
   * @param {Event} event
   *        The key event.
   * @param {number} numListItems
   *        The number of items in the list.  The reason that this is a
   *        parameter at all is that the list may contain items at the end
   *        that should be ignored, depending on the consumer.  That's true
   *        for the urlbar for example.
   * @param {boolean} allowEmptySelection
   *        Pass true if it's OK that neither the list nor the one-off
   *        buttons contains a selection.  Pass false if either the list or
   *        the one-off buttons (or both) should always contain a selection.
   * @param {string} [textboxUserValue]
   *        When the last list item is selected and the user presses Down,
   *        the first one-off becomes selected and the textbox value is
   *        restored to the value that the user typed.  Pass that value here.
   *        However, if you pass true for allowEmptySelection, you don't need
   *        to pass anything for this parameter.  (Pass undefined or null.)
   * @returns {boolean} True if the one-offs handled the key press.
   */
  handleKeyDown(event, numListItems, allowEmptySelection, textboxUserValue) {
    if (!this.hasView) {
      return false;
    }
    let handled = this._handleKeyDown(
      event,
      numListItems,
      allowEmptySelection,
      textboxUserValue
    );
    if (handled) {
      event.preventDefault();
      event.stopPropagation();
    }
    return handled;
  }

  _handleKeyDown(event, numListItems, allowEmptySelection, textboxUserValue) {
    if (this.compact && this.container.hidden) {
      return false;
    }
    if (
      event.keyCode == KeyEvent.DOM_VK_RIGHT &&
      this.selectedButton &&
      this.selectedButton.classList.contains("addengine-menu-button")
    ) {
      // If the add-engine overflow menu item is selected and the user
      // presses the right arrow key, open the submenu.  Unfortunately
      // handling the left arrow key -- to close the popup -- isn't
      // straightforward.  Once the popup is open, it consumes all key
      // events.  Setting ignorekeys=handled on it doesn't help, since the
      // popup handles all arrow keys.  Setting ignorekeys=true on it does
      // mean that the popup no longer consumes the left arrow key, but
      // then it no longer handles up/down keys to select items in the
      // popup.
      this.selectedButton.open = true;
      return true;
    }

    // Handle the Tab key, but only if non-Shift modifiers aren't also
    // pressed to avoid clobbering other shortcuts (like the Alt+Tab
    // browser tab switcher).  The reason this uses getModifierState() and
    // checks for "AltGraph" is that when you press Shift-Alt-Tab,
    // event.altKey is actually false for some reason, at least on macOS.
    // getModifierState("Alt") is also false, but "AltGraph" is true.
    if (
      event.keyCode == KeyEvent.DOM_VK_TAB &&
      !event.getModifierState("Alt") &&
      !event.getModifierState("AltGraph") &&
      !event.getModifierState("Control") &&
      !event.getModifierState("Meta")
    ) {
      if (
        this.getAttribute("disabletab") == "true" ||
        (event.shiftKey && this.selectedButtonIndex <= 0) ||
        (!event.shiftKey &&
          this.selectedButtonIndex ==
            this.getSelectableButtons(true).length - 1)
      ) {
        this.selectedButton = null;
        return false;
      }
      this.selectedViewIndex = -1;
      this.advanceSelection(!event.shiftKey, true, false);
      return !!this.selectedButton;
    }

    if (event.keyCode == KeyboardEvent.DOM_VK_UP) {
      if (event.altKey) {
        // Keep the currently selected result in the list (if any) as a
        // secondary "alt" selection and move the selection up within the
        // buttons.
        this.advanceSelection(false, false, false);
        return true;
      }
      if (numListItems == 0) {
        this.advanceSelection(false, true, false);
        return true;
      }
      if (this.selectedViewIndex > 0) {
        // Moving up within the list.  The autocomplete controller should
        // handle this case.  A button may be selected, so null it.
        this.selectedButton = null;
        return false;
      }
      if (this.selectedViewIndex == 0) {
        // Moving up from the top of the list.
        if (allowEmptySelection) {
          // Let the autocomplete controller remove selection in the list
          // and revert the typed text in the textbox.
          return false;
        }
        // Wrap selection around to the last button.
        if (this.textbox && typeof textboxUserValue == "string") {
          this.textbox.value = textboxUserValue;
        }
        this.selectedViewIndex = -1;
        this.advanceSelection(false, true, true);
        return true;
      }
      if (!this.selectedButton) {
        // Moving up from no selection in the list or the buttons, back
        // down to the last button.
        this.advanceSelection(false, true, true);
        return true;
      }
      if (this.selectedButtonIndex == 0) {
        // Moving up from the buttons to the bottom of the list.
        this.selectedButton = null;
        return false;
      }
      // Moving up/left within the buttons.
      this.advanceSelection(false, true, false);
      return true;
    }

    if (event.keyCode == KeyboardEvent.DOM_VK_DOWN) {
      if (event.altKey) {
        // Keep the currently selected result in the list (if any) as a
        // secondary "alt" selection and move the selection down within
        // the buttons.
        this.advanceSelection(true, false, false);
        return true;
      }
      if (numListItems == 0) {
        this.advanceSelection(true, true, false);
        return true;
      }
      if (
        this.selectedViewIndex >= 0 &&
        this.selectedViewIndex < numListItems - 1
      ) {
        // Moving down within the list.  The autocomplete controller
        // should handle this case.  A button may be selected, so null it.
        this.selectedButton = null;
        return false;
      }
      if (this.selectedViewIndex == numListItems - 1) {
        // Moving down from the last item in the list to the buttons.
        if (!allowEmptySelection) {
          this.selectedViewIndex = -1;
          if (this.textbox && typeof textboxUserValue == "string") {
            this.textbox.value = textboxUserValue;
          }
        }
        this.selectedButtonIndex = 0;
        if (allowEmptySelection) {
          // Let the autocomplete controller remove selection in the list
          // and revert the typed text in the textbox.
          return false;
        }
        return true;
      }
      if (this.selectedButton) {
        let buttons = this.getSelectableButtons(true);
        if (this.selectedButtonIndex == buttons.length - 1) {
          // Moving down from the buttons back up to the top of the list.
          this.selectedButton = null;
          if (allowEmptySelection) {
            // Prevent the selection from wrapping around to the top of
            // the list by returning true, since the list currently has no
            // selection.  Nothing should be selected after handling this
            // Down key.
            return true;
          }
          return false;
        }
        // Moving down/right within the buttons.
        this.advanceSelection(true, true, false);
        return true;
      }
      return false;
    }

    if (event.keyCode == KeyboardEvent.DOM_VK_LEFT) {
      if (
        this.selectedButton &&
        (this.compact || this.selectedButton.engine) &&
        !this.disableOneOffsHorizontalKeyNavigation
      ) {
        // Moving left within the buttons.
        this.advanceSelection(false, this.compact, true);
        return true;
      }
      return false;
    }

    if (event.keyCode == KeyboardEvent.DOM_VK_RIGHT) {
      if (
        this.selectedButton &&
        (this.compact || this.selectedButton.engine) &&
        !this.disableOneOffsHorizontalKeyNavigation
      ) {
        // Moving right within the buttons.
        this.advanceSelection(true, this.compact, true);
        return true;
      }
      return false;
    }

    return false;
  }

  /**
   * Determines if the target of the event is a one-off button or
   * context menu on a one-off button.
   *
   * @param {Event} event
   *        An event, like a click on a one-off button.
   * @returns {boolean} True if telemetry was recorded and false if not.
   */
  eventTargetIsAOneOff(event) {
    if (!event) {
      return false;
    }

    let target = event.originalTarget;

    if (event instanceof KeyboardEvent && this.selectedButton) {
      return true;
    }
    if (
      event instanceof MouseEvent &&
      target.classList.contains("searchbar-engine-one-off-item")
    ) {
      return true;
    }
    if (
      event instanceof this.window.XULCommandEvent &&
      target.classList.contains("search-one-offs-context-open-in-new-tab")
    ) {
      return true;
    }

    return false;
  }

  _resetAddEngineMenuTimeout() {
    if (this._addEngineMenuTimeout) {
      clearTimeout(this._addEngineMenuTimeout);
    }
    this._addEngineMenuTimeout = setTimeout(() => {
      delete this._addEngineMenuTimeout;
      let button = this.querySelector(".addengine-menu-button");
      button.open = this._addEngineMenuShouldBeOpen;
    }, this._addEngineMenuTimeoutMs);
  }

  // Methods for subclasses to override

  /**
   * @returns {boolean} True if the one-offs are connected to a view.
   */
  get hasView() {
    return !!this.popup;
  }

  /**
   * @returns {boolean} True if the view is open.
   */
  get isViewOpen() {
    return this.popup && this.popup.popupOpen;
  }

  /**
   * @returns {number} The selected index in the view or -1 if no selection.
   */
  get selectedViewIndex() {
    return this.popup.selectedIndex;
  }

  /**
   * Sets the selected index in the view.
   *
   * @param {number} val
   *        The selected index or -1 if no selection.
   */
  set selectedViewIndex(val) {
    this.popup.selectedIndex = val;
  }

  /**
   * Closes the view.
   */
  closeView() {
    this.popup.hidePopup();
  }

  /**
   * Called when a one-off is clicked or the "Search in New Tab" context menu
   * item is picked.  This is not called for the settings button.
   *
   * @param {event} event
   *        The event that triggered the pick.
   * @param {nsISearchEngine|SearchEngine} engine
   *        The engine that was picked.
   * @param {boolean} forceNewTab
   *        True if the search results page should be loaded in a new tab.
   */
  handleSearchCommand(event, engine, forceNewTab = false) {
    let { where, params } = this._whereToOpen(event, forceNewTab);
    this.popup.handleOneOffSearch(event, engine, where, params);
  }

  /**
   * Sets the tooltip for a one-off button with an engine.  This should set
   * either the `tooltiptext` attribute or the relevant l10n ID.
   *
   * @param {element} button
   *        The one-off button.
   */
  setTooltipForEngineButton(button) {
    button.setAttribute("tooltiptext", button.engine.name);
  }

  // Event handlers below.

  _on_mousedown(event) {
    let target = event.originalTarget;
    if (target.classList.contains("addengine-menu-button")) {
      return;
    }
    // This is necessary to prevent the input from losing focus and closing the
    // popup. Unfortunately it also has the side effect of preventing the
    // buttons from receiving the `:active` pseudo-class.
    event.preventDefault();
  }

  _on_mousemove(event) {
    let target = event.originalTarget;

    // Handle mouseover on the add-engine menu button and its popup items.
    if (
      (target.localName == "menuitem" &&
        target.classList.contains("addengine-item")) ||
      target.classList.contains("addengine-menu-button")
    ) {
      this._addEngineMenuShouldBeOpen = true;
      this._resetAddEngineMenuTimeout();
    }
  }

  _on_mouseout(event) {
    let target = event.originalTarget;

    // Handle mouseout on the add-engine menu button and its popup items.
    if (
      (target.localName == "menuitem" &&
        target.classList.contains("addengine-item")) ||
      target.classList.contains("addengine-menu-button")
    ) {
      this._addEngineMenuShouldBeOpen = false;
      this._resetAddEngineMenuTimeout();
    }
  }

  _on_click(event) {
    if (event.button == 2) {
      return; // ignore right clicks.
    }

    let button = event.originalTarget;
    let engine = button.engine;

    if (!engine) {
      return;
    }

    // Select the clicked button so that consumers can easily tell which
    // button was acted on.
    this.selectedButton = button;
    this.handleSearchCommand(event, engine);
  }

  _on_command(event) {
    let target = event.target;

    if (target == this.settingsButton || target == this.settingsButtonCompact) {
      this.window.openPreferences("paneSearch");

      // If the preference tab was already selected, the panel doesn't
      // close itself automatically.
      this.closeView();
      return;
    }

    if (target.classList.contains("addengine-item")) {
      // On success, hide the panel and tell event listeners to reshow it to
      // show the new engine.
      SearchUIUtils.addOpenSearchEngine(
        target.getAttribute("uri"),
        target.getAttribute("image"),
        this.window.gBrowser.selectedBrowser.browsingContext
      )
        .then(result => {
          if (result) {
            this._rebuild();
          }
        })
        .catch(console.error);
    }

    if (target.classList.contains("search-one-offs-context-open-in-new-tab")) {
      // Select the context-clicked button so that consumers can easily
      // tell which button was acted on.
      this.selectedButton = this._buttonForEngine(this._contextEngine);
      this.handleSearchCommand(event, this._contextEngine, true);
    }

    const isPrivateButton = target.classList.contains(
      "search-one-offs-context-set-default-private"
    );
    if (
      target.classList.contains("search-one-offs-context-set-default") ||
      isPrivateButton
    ) {
      const engineType = isPrivateButton
        ? "defaultPrivateEngine"
        : "defaultEngine";
      let currentEngine = Services.search[engineType];

      const isPrivateWin = PrivateBrowsingUtils.isWindowPrivate(this.window);
      if (
        !this.getAttribute("includecurrentengine") &&
        isPrivateButton == isPrivateWin
      ) {
        // Make the target button of the context menu reflect the current
        // search engine first. Doing this as opposed to rebuilding all the
        // one-off buttons avoids flicker.
        let button = this._buttonForEngine(this._contextEngine);
        button.id = this._buttonIDForEngine(currentEngine);
        let uri = "chrome://browser/skin/search-engine-placeholder.png";
        if (currentEngine.iconURI) {
          uri = currentEngine.iconURI.spec;
        }
        button.setAttribute("image", uri);
        button.setAttribute("tooltiptext", currentEngine.name);
        button.engine = currentEngine;
      }

      Services.search[engineType] = this._contextEngine;
    }
  }

  _on_contextmenu(event) {
    let target = event.originalTarget;
    // Prevent the context menu from appearing except on the one off buttons.
    if (
      !target.classList.contains("searchbar-engine-one-off-item") ||
      target.classList.contains("search-setting-button-compact")
    ) {
      event.preventDefault();
      return;
    }
    this.contextMenuPopup
      .querySelector(".search-one-offs-context-set-default")
      .setAttribute(
        "disabled",
        target.engine == Services.search.defaultEngine.wrappedJSObject
      );

    const privateDefaultItem = this.contextMenuPopup.querySelector(
      ".search-one-offs-context-set-default-private"
    );

    if (
      Services.prefs.getBoolPref(
        "browser.search.separatePrivateDefault.ui.enabled",
        false
      ) &&
      Services.prefs.getBoolPref("browser.search.separatePrivateDefault", false)
    ) {
      privateDefaultItem.hidden = false;
      privateDefaultItem.setAttribute(
        "disabled",
        target.engine == Services.search.defaultPrivateEngine.wrappedJSObject
      );
    } else {
      privateDefaultItem.hidden = true;
    }

    this.contextMenuPopup.openPopupAtScreen(event.screenX, event.screenY, true);
    event.preventDefault();

    this._contextEngine = target.engine;
  }

  _on_input(event) {
    // Allow the consumer's input to override its value property with
    // a oneOffSearchQuery property.  That way if the value is not
    // actually what the user typed (e.g., it's autofilled, or it's a
    // mozaction URI), the consumer has some way of providing it.
    this.query = event.target.oneOffSearchQuery || event.target.value;
  }

  _on_popupshowing() {
    this._rebuild();
  }

  _on_popuphidden() {
    this.selectedButton = null;
    this._contextEngine = null;
  }
}
