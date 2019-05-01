/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* eslint-env mozilla/browser-window */
/* globals XULCommandEvent */

/**
 * Defines the search one-off button elements. These are displayed at the bottom
 * of the address bar or the search bar.
 */
class SearchOneOffs {
  constructor(container) {
    this.container = container;

    this.container.appendChild(MozXULElement.parseXULToFragment(`
      <deck class="search-panel-one-offs-header search-panel-header search-panel-current-input">
        <label class="searchbar-oneoffheader-search" value="&searchWithHeader.label;"/>
        <hbox class="search-panel-searchforwith search-panel-current-input">
          <label value="&searchFor.label;"/>
          <label class="searchbar-oneoffheader-searchtext search-panel-input-value" flex="1" crop="end"/>
          <label flex="10000" value="&searchWith.label;"/>
        </hbox>
        <hbox class="search-panel-searchonengine search-panel-current-input">
          <label value="&search.label;"/>
          <label class="searchbar-oneoffheader-engine search-panel-input-value" flex="1" crop="end"/>
          <label flex="10000" value="&searchAfter.label;"/>
        </hbox>
      </deck>
      <description role="group" class="search-panel-one-offs">
        <button class="searchbar-engine-one-off-item search-setting-button-compact" tooltiptext="&changeSearchSettings.tooltip;"/>
      </description>
      <vbox class="search-add-engines"/>
      <button class="search-setting-button search-panel-header" label="&changeSearchSettings.button;"/>
      <menupopup class="search-one-offs-context-menu">
        <menuitem class="search-one-offs-context-open-in-new-tab" label="&searchInNewTab.label;" accesskey="&searchInNewTab.accesskey;"/>
        <menuitem class="search-one-offs-context-set-default" label="&searchSetAsDefault.label;" accesskey="&searchSetAsDefault.accesskey;"/>
      </menupopup>
      `, ["chrome://browser/locale/browser.dtd"]));

    this._view = null;
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

    this.settingsButtonCompact = this.querySelector(".search-setting-button-compact");

    this.contextMenuPopup = this.querySelector(".search-one-offs-context-menu");

    this._bundle = null;

    /**
     * When a context menu is opened on a one-off button, this is set to the
     * engine of that button for use with the context menu actions.
     */
    this._contextEngine = null;

    this._engines = null;

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
      this._ignoreMouseEvents = true;
      aEvent.stopPropagation();
    });
    this.contextMenuPopup.addEventListener("popuphidden", aEvent => {
      this._ignoreMouseEvents = false;
      aEvent.stopPropagation();
    });

    // Add weak referenced observers to invalidate our cached list of engines.
    this.QueryInterface = ChromeUtils.generateQI([Ci.nsIObserver, Ci.nsISupportsWeakReference]);
    Services.prefs.addObserver("browser.search.hiddenOneOffs", this, true);
    Services.obs.addObserver(this, "browser-search-engine-modified", true);
    Services.obs.addObserver(this, "browser-search-service", true);

    // Rebuild the buttons when the theme changes.  See bug 1357800 for
    // details.  Summary: On Linux, switching between themes can cause a row
    // of buttons to disappear.
    Services.obs.addObserver(this, "lightweight-theme-changed", true);
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
   * Width in pixels of the one-off buttons.  49px is the min-width of
   * each search engine button, adapt this const when changing the css.
   * It's actually 48px + 1px of right border.
   */
  get buttonWidth() {
    return 49;
  }

  /**
   * @param {UrlbarView} val
   */
  set view(val) {
    this._view = val;
    this.popup = val && val.panel;
    return val;
  }

  /**
   * The popup that contains the one-offs.  This is required, so it should
   * never be null or undefined, except possibly before the one-offs are
   * used.
   *
   * @param {DOMElement} val
   *        The new value to set.
   */
  set popup(val) {
    let events = [
      "popupshowing",
      "popuphidden",
    ];
    if (this._popup) {
      for (let event of events) {
        this._popup.removeEventListener(event, this);
      }
    }
    if (val) {
      for (let event of events) {
        val.addEventListener(event, this);
      }
    }
    this._popup = val;

    // If the popup is already open, rebuild the one-offs now.  The
    // popup may be opening, so check that the state is not closed
    // instead of checking popupOpen.
    if (val && val.state != "closed") {
      this._rebuild();
    }

    return val;
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
    return this._textbox = val;
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
    if (this._view && this._view.isOpen ||
        this.popup && this.popup.popupOpen) {
      this._updateAfterQueryChanged();
    }
    return val;
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
    if (val && val.classList.contains("dummy")) {
      // Never select dummy buttons.
      val = null;
    }
    let previousButton = this._selectedButton;
    if (previousButton) {
      previousButton.removeAttribute("selected");
    }
    if (val) {
      val.setAttribute("selected", "true");
    }
    this._selectedButton = val;
    this._updateStateForButton(null);
    if (val && !val.engine) {
      // If the button doesn't have an engine, then clear the popup's
      // selection to indicate that pressing Return while the button is
      // selected will do the button's command, not search.
      this.selectedAutocompleteIndex = -1;
    }
    let event = new CustomEvent("SelectedOneOffButtonChanged", {
      previousSelectedButton: previousButton,
    });
    this.dispatchEvent(event);
    return val;
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
    return val;
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

  get selectedAutocompleteIndex() {
    return (this._view || this.popup).selectedIndex;
  }

  set selectedAutocompleteIndex(val) {
    return (this._view || this.popup).selectedIndex = val;
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

  async getEngines() {
    if (this._engines) {
      return this._engines;
    }
    let currentEngineNameToIgnore;
    if (!this.getAttribute("includecurrentengine"))
      currentEngineNameToIgnore = (await Services.search.getDefault()).name;

    let pref = Services.prefs.getStringPref("browser.search.hiddenOneOffs");
    let hiddenList = pref ? pref.split(",") : [];

    this._engines = (await Services.search.getVisibleEngines()).filter(e => {
      let name = e.name;
      return (!currentEngineNameToIgnore ||
              name != currentEngineNameToIgnore) &&
             !hiddenList.includes(name);
    });

    return this._engines;
  }

  observe(aEngine, aTopic, aData) {
    // Make sure the engine list is refetched next time it's needed.
    this._engines = null;
  }

  /**
   * Updates the parts of the UI that show the query string.
   */
  _updateAfterQueryChanged() {
    let headerSearchText = this.querySelector(".searchbar-oneoffheader-searchtext");
    headerSearchText.setAttribute("value", this.query);
    let groupText;
    let isOneOffSelected =
      this.selectedButton &&
      this.selectedButton.classList.contains("searchbar-engine-one-off-item");
    // Typing de-selects the settings or opensearch buttons at the bottom
    // of the search panel, as typing shows the user intends to search.
    if (this.selectedButton && !isOneOffSelected) {
      this.selectedButton = null;
    }
    if (this.query) {
      groupText = headerSearchText.previousElementSibling.value +
                  '"' + headerSearchText.value + '"' +
                  headerSearchText.nextElementSibling.value;
      if (!isOneOffSelected) {
        this.header.selectedIndex = 1;
      }
    } else {
      let noSearchHeader = this.querySelector(".searchbar-oneoffheader-search");
      groupText = noSearchHeader.value;
      if (!isOneOffSelected) {
        this.header.selectedIndex = 0;
      }
    }
    this.buttons.setAttribute("aria-label", groupText);
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
    // Update the 'Search for <keywords> with:" header.
    this._updateAfterQueryChanged();

    // Handle opensearch items. This needs to be done before building the
    // list of one off providers, as that code will return early if all the
    // alternative engines are hidden.
    // Skip this in compact mode, ie. for the urlbar.
    if (!this.compact) {
      this._rebuildAddEngineList();
    }

    // Check if the one-off buttons really need to be rebuilt.
    if (this._textbox) {
      // We can't get a reliable value for the popup width without flushing,
      // but the popup width won't change if the textbox width doesn't.
      let DOMUtils = window.windowUtils;
      let textboxWidth = DOMUtils.getBoundsWithoutFlushing(this._textbox).width;
      // We can return early if neither the list of engines nor the panel
      // width has changed.
      if (this._engines && this._textboxWidth == textboxWidth) {
        return;
      }
      this._textboxWidth = textboxWidth;
    }

    // Finally, build the list of one-off buttons.
    while (this.buttons.firstElementChild != this.settingsButtonCompact) {
      this.buttons.firstElementChild.remove();
    }

    // Remove the trailing empty text node introduced by the binding's
    // content markup above.
    if (this.settingsButtonCompact.nextElementSibling) {
      this.settingsButtonCompact.nextElementSibling.remove();
    }

    let engines = await this.getEngines();
    let defaultEngine = await Services.search.getDefault();
    let oneOffCount = engines.length;
    let collapsed = !oneOffCount ||
                    (oneOffCount == 1 && engines[0].name == defaultEngine.name);

    // header is a xul:deck so collapsed doesn't work on it, see bug 589569.
    this.header.hidden = this.buttons.collapsed = collapsed;

    if (collapsed) {
      return;
    }

    let panelWidth = parseInt(this.popup.clientWidth);

    // There's one weird thing to guard against: when layout pixels
    // aren't an integral multiple of device pixels, the last button
    // of each row sometimes gets pushed to the next row, depending on the
    // panel and button widths.
    // This is likely because the clientWidth getter rounds the value, but
    // the panel's border width is not an integer.
    // As a workaround, decrement the width if the scale is not an integer.
    let scale = window.windowUtils.screenPixelsPerCSSPixel;
    if (Math.floor(scale) != scale) {
      --panelWidth;
    }

    // The + 1 is because the last button doesn't have a right border.
    let enginesPerRow = Math.floor((panelWidth + 1) / this.buttonWidth);
    let buttonWidth = Math.floor(panelWidth / enginesPerRow);
    // There will be an emtpy area of:
    //   panelWidth - enginesPerRow * buttonWidth  px
    // at the end of each row.

    // If the <description> tag with the list of search engines doesn't have
    // a fixed height, the panel will be sized incorrectly, causing the bottom
    // of the suggestion <tree> to be hidden.
    if (this.compact) {
      ++oneOffCount;
    }
    let rowCount = Math.ceil(oneOffCount / enginesPerRow);
    let height = rowCount * 33; // 32px per row, 1px border.
    this.buttons.setAttribute("height", height + "px");

    // Ensure we can refer to the settings buttons by ID:
    let origin = this.telemetryOrigin;
    this.settingsButton.id = origin + "-anon-search-settings";
    this.settingsButtonCompact.id = origin + "-anon-search-settings-compact";

    let dummyItems = enginesPerRow - (oneOffCount % enginesPerRow || enginesPerRow);
    for (let i = 0; i < engines.length; ++i) {
      let engine = engines[i];
      let button = document.createXULElement("button");
      button.id = this._buttonIDForEngine(engine);
      let uri = "chrome://browser/skin/search-engine-placeholder.png";
      if (engine.iconURI) {
        uri = engine.iconURI.spec;
      }
      button.setAttribute("image", uri);
      button.setAttribute("class", "searchbar-engine-one-off-item");
      button.setAttribute("tooltiptext", engine.name);
      button.setAttribute("width", buttonWidth);
      button.engine = engine;

      if ((i + 1) % enginesPerRow == 0) {
        button.classList.add("last-of-row");
      }

      if (i + 1 == engines.length) {
        button.classList.add("last-engine");
      }

      if (i >= oneOffCount + dummyItems - enginesPerRow) {
        button.classList.add("last-row");
      }

      this.buttons.insertBefore(button, this.settingsButtonCompact);
    }

    let hasDummyItems = !!dummyItems;
    while (dummyItems) {
      let button = document.createXULElement("button");
      button.setAttribute("class", "searchbar-engine-one-off-item dummy last-row");
      button.setAttribute("width", buttonWidth);

      if (!--dummyItems) {
        button.classList.add("last-of-row");
      }

      this.buttons.insertBefore(button, this.settingsButtonCompact);
    }

    if (this.compact) {
      this.settingsButtonCompact.setAttribute("width", buttonWidth);
      if (rowCount == 1 && hasDummyItems) {
        // When there's only one row, make the compact settings button
        // hug the right edge of the panel.  It may not due to the panel's
        // width not being an integral multiple of the button width.  (See
        // the "There will be an emtpy area" comment above.)  Increase the
        // width of the last dummy item by the remainder.
        let remainder = panelWidth - (enginesPerRow * buttonWidth);
        let width = remainder + buttonWidth;
        let lastDummyItem = this.settingsButtonCompact.previousElementSibling;
        lastDummyItem.setAttribute("width", width);
      }
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

    if (!gBrowser.selectedBrowser.engines) {
      return;
    }

    let engines = gBrowser.selectedBrowser.engines;
    let tooManyEngines = engines.length > this._addEngineMenuThreshold;

    if (tooManyEngines) {
      // Make the top-level menu button.
      let button = document.createXULElement("toolbarbutton");
      list.appendChild(button);
      button.classList.add("addengine-menu-button", "addengine-item",
                           "badged-button");
      button.setAttribute("type", "menu");
      button.setAttribute("label",
        this.bundle.GetStringFromName("cmd_addFoundEngineMenu"));
      button.setAttribute("crop", "end");
      button.setAttribute("pack", "start");

      // Set the menu button's image to the image of the first engine.  The
      // offered engines may have differing images, so there's no perfect
      // choice here.
      let engine = engines[0];
      if (engine.icon) {
        button.setAttribute("image", engine.icon);
      }

      // Now make the button's child menupopup.
      list = document.createXULElement("menupopup");
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
      let button = document.createXULElement(eltType);
      button.classList.add("addengine-item");
      if (!tooManyEngines) {
        button.classList.add("badged-button");
      }
      button.id = this.telemetryOrigin + "-add-engine-" +
        this._fixUpEngineNameForID(engine.title);
      let label = this.bundle.formatStringFromName("cmd_addFoundEngine", [engine.title], 1);
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
    return this.telemetryOrigin + "-engine-one-off-item-" + this._fixUpEngineNameForID(engine.name);
  }

  _fixUpEngineNameForID(name) {
    return name.replace(/ /g, "-");
  }

  _buttonForEngine(engine) {
    if (this.telemetryOrigin == "urlbar" && !UrlbarPrefs.get("quantumbar")) {
      return this._popup &&
             document.getAnonymousElementByAttribute(this._popup, "id", this._buttonIDForEngine(engine));
    }

    let id = this._buttonIDForEngine(engine);
    return this._popup && document.getElementById(id);
  }

  /**
   * Updates the popup and textbox for the currently selected or moused-over
   * button.
   *
   * @param {DOMElement} mousedOverButton
   *        The currently moused-over button, or null if there isn't one.
   */
  _updateStateForButton(mousedOverButton) {
    let button = mousedOverButton;

    // Ignore dummy buttons.
    if (button && button.classList.contains("dummy")) {
      button = null;
    }

    // If there's no moused-over button, then the one-offs should reflect
    // the selected button, if any.
    button = button || this.selectedButton;

    if (!button) {
      this.header.selectedIndex = this.query ? 1 : 0;
      if (this.textbox) {
        this.textbox.removeAttribute("aria-activedescendant");
      }
      return;
    }

    if (button.classList.contains("searchbar-engine-one-off-item") && button.engine) {
      let headerEngineText = this.querySelector(".searchbar-oneoffheader-engine");
      this.header.selectedIndex = 2;
      headerEngineText.value = button.engine.name;
    } else {
      this.header.selectedIndex = this.query ? 1 : 0;
    }
    if (this.textbox) {
      this.textbox.setAttribute("aria-activedescendant", button.id);
    }
  }

  getSelectableButtons(aIncludeNonEngineButtons) {
    let buttons = [];
    for (let oneOff = this.buttons.firstElementChild; oneOff; oneOff = oneOff.nextElementSibling) {
      // oneOff may be a text node since the list xul:description contains
      // whitespace and the compact settings button.  See the markup
      // above.  _rebuild removes text nodes, but it may not have been
      // called yet (because e.g. the popup hasn't been opened yet).
      if (oneOff.nodeType == Node.ELEMENT_NODE) {
        if (oneOff.classList.contains("dummy") ||
            oneOff.classList.contains("search-setting-button-compact")) {
          break;
        }
        buttons.push(oneOff);
      }
    }

    if (aIncludeNonEngineButtons) {
      for (let addEngine = this.addEngines.firstElementChild; addEngine; addEngine = addEngine.nextElementSibling) {
        buttons.push(addEngine);
      }
      buttons.push(this.compact ? this.settingsButtonCompact : this.settingsButton);
    }

    return buttons;
  }

  handleSearchCommand(aEvent, aEngine, aForceNewTab) {
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
      if ((aEvent instanceof KeyboardEvent && aEvent.altKey) ^ newTabPref &&
          !gBrowser.selectedTab.isEmpty) {
        where = "tab";
      }
      if (aEvent instanceof MouseEvent &&
          (aEvent.button == 1 || aEvent.getModifierState("Accel"))) {
        where = "tab";
        params = {
          inBackground: true,
        };
      }
    }

    (this._view || this.popup).handleOneOffSearch(aEvent, aEngine, where, params);
  }

  /**
   * Increments or decrements the index of the currently selected one-off.
   *
   * @param {boolean} aForward
   *        If true, the index is incremented, and if false, the index is
   *        decremented.
   * @param {boolean} aIncludeNonEngineButtons
   *        If true, non-dummy buttons that do not have engines are included.
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
      if (!aWrapAround &&
          ((aForward && index <= oldIndex) ||
           (!aForward && oldIndex <= index))) {
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
  handleKeyPress(event, numListItems, allowEmptySelection, textboxUserValue) {
    if (!this.popup) {
      return false;
    }
    let handled = this._handleKeyPress(event, numListItems,
      allowEmptySelection,
      textboxUserValue);
    if (handled) {
      event.preventDefault();
      event.stopPropagation();
    }
    return handled;
  }

  _handleKeyPress(event, numListItems, allowEmptySelection, textboxUserValue) {
    if (this.compact && this.buttons.collapsed) {
      return false;
    }
    if (event.keyCode == KeyEvent.DOM_VK_RIGHT &&
        this.selectedButton &&
        this.selectedButton.classList.contains("addengine-menu-button")) {
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
    if (event.keyCode == KeyEvent.DOM_VK_TAB &&
        !event.getModifierState("Alt") &&
        !event.getModifierState("AltGraph") &&
        !event.getModifierState("Control") &&
        !event.getModifierState("Meta")) {
      if (this.getAttribute("disabletab") == "true" ||
          (event.shiftKey && this.selectedButtonIndex <= 0) ||
          (!event.shiftKey && this.selectedButtonIndex == this.getSelectableButtons(true).length - 1)) {
        this.selectedButton = null;
        return false;
      }
      this.selectedAutocompleteIndex = -1;
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
      if (this.selectedAutocompleteIndex > 0) {
        // Moving up within the list.  The autocomplete controller should
        // handle this case.  A button may be selected, so null it.
        this.selectedButton = null;
        return false;
      }
      if (this.selectedAutocompleteIndex == 0) {
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
      if (this.selectedAutocompleteIndex >= 0 &&
          this.selectedAutocompleteIndex < numListItems - 1) {
        // Moving down within the list.  The autocomplete controller
        // should handle this case.  A button may be selected, so null it.
        this.selectedButton = null;
        return false;
      }
      if (this.selectedAutocompleteIndex == numListItems - 1) {
        // Moving down from the last item in the list to the buttons.
        this.selectedButtonIndex = 0;
        if (allowEmptySelection) {
          // Let the autocomplete controller remove selection in the list
          // and revert the typed text in the textbox.
          return false;
        }
        if (this.textbox && typeof textboxUserValue == "string") {
          this.textbox.value = textboxUserValue;
        }
        this.selectedAutocompleteIndex = -1;
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
      if (this.selectedButton && (this.compact || this.selectedButton.engine)) {
        // Moving left within the buttons.
        this.advanceSelection(false, this.compact, true);
        return true;
      }
      return false;
    }

    if (event.keyCode == KeyboardEvent.DOM_VK_RIGHT) {
      if (this.selectedButton && (this.compact || this.selectedButton.engine)) {
        // Moving right within the buttons.
        this.advanceSelection(true, this.compact, true);
        return true;
      }
      return false;
    }

    return false;
  }

  /**
   * If the given event is related to the one-offs, this method records
   * one-off telemetry for it.  this.telemetryOrigin will be appended to the
   * computed source, so make sure you set that first.
   *
   * @param {Event} aEvent
   *        An event, like a click on a one-off button.
   * @returns {boolean} True if telemetry was recorded and false if not.
   */
  maybeRecordTelemetry(aEvent) {
    if (!aEvent) {
      return false;
    }

    let source = null;
    let type = "unknown";
    let engine = null;
    let target = aEvent.originalTarget;

    if (aEvent instanceof KeyboardEvent) {
      type = "key";
      if (this.selectedButton) {
        source = "oneoff";
        engine = this.selectedButton.engine;
      }
    } else if (aEvent instanceof MouseEvent) {
      type = "mouse";
      if (target.classList.contains("searchbar-engine-one-off-item")) {
        source = "oneoff";
        engine = target.engine;
      }
    } else if (aEvent instanceof XULCommandEvent &&
               target.classList.contains("search-one-offs-context-open-in-new-tab")) {
      source = "oneoff-context";
      engine = this._contextEngine;
    }

    if (!source) {
      return false;
    }

    if (this.telemetryOrigin) {
      source += "-" + this.telemetryOrigin;
    }

    BrowserSearch.recordOneoffSearchInTelemetry(engine, source, type);
    return true;
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

  // Event handlers below.

  _on_mousedown(event) {
    let target = event.originalTarget;
    if (target.classList.contains("addengine-menu-button")) {
      return;
    }
    // Required to receive click events from the buttons on Linux.
    event.preventDefault();
  }

  _on_mousemove(event) {
    let target = event.originalTarget;

    // Handle mouseover on the add-engine menu button and its popup items.
    if ((target.localName == "menuitem" && target.classList.contains("addengine-item")) ||
        target.classList.contains("addengine-menu-button")) {
      let menuButton = this.querySelector(".addengine-menu-button");
      this._updateStateForButton(menuButton);
      this._addEngineMenuShouldBeOpen = true;
      this._resetAddEngineMenuTimeout();
      return;
    }

    if (target.localName != "button") {
      return;
    }

    // Ignore mouse events when the context menu is open.
    if (this._ignoreMouseEvents) {
      return;
    }

    let isOneOff =
      target.classList.contains("searchbar-engine-one-off-item") &&
      !target.classList.contains("dummy");
    if (isOneOff ||
        target.classList.contains("addengine-item") ||
        target.classList.contains("search-setting-button")) {
      this._updateStateForButton(target);
    }
  }

  _on_mouseout(event) {
    let target = event.originalTarget;

    // Handle mouseout on the add-engine menu button and its popup items.
    if ((target.localName == "menuitem" && target.classList.contains("addengine-item")) ||
        target.classList.contains("addengine-menu-button")) {
      this._updateStateForButton(null);
      this._addEngineMenuShouldBeOpen = false;
      this._resetAddEngineMenuTimeout();
      return;
    }

    if (target.localName != "button") {
      return;
    }

    // Don't update the mouseover state if the context menu is open.
    if (this._ignoreMouseEvents) {
      return;
    }

    this._updateStateForButton(null);
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

    if (target == this.settingsButton ||
        target == this.settingsButtonCompact) {
      openPreferences("paneSearch");

      // If the preference tab was already selected, the panel doesn't
      // close itself automatically.
      this.popup.hidePopup();
      return;
    }

    if (target.classList.contains("addengine-item")) {
      // On success, hide the panel and tell event listeners to reshow it to
      // show the new engine.
      Services.search.addEngine(target.getAttribute("uri"), target.getAttribute("image"), false)
        .then(engine => {
          this._rebuild();
        })
        .catch(errorCode => {
          if (errorCode != Ci.nsISearchService.ERROR_DUPLICATE_ENGINE) {
            // Download error is shown by the search service
            return;
          }
          const kSearchBundleURI =
            "chrome://global/locale/search/search.properties";
          let searchBundle = Services.strings.createBundle(kSearchBundleURI);
          let brandBundle = document.getElementById("bundle_brand");
          let brandName = brandBundle.getString("brandShortName");
          let title = searchBundle.GetStringFromName(
            "error_invalid_engine_title"
          );
          let text = searchBundle.formatStringFromName(
            "error_duplicate_engine_msg",
            [brandName, target.getAttribute("uri")],
            2
          );
          Services.prompt.QueryInterface(Ci.nsIPromptFactory);
          let prompt = Services.prompt.getPrompt(
            gBrowser.contentWindow,
            Ci.nsIPrompt
          );
          prompt.QueryInterface(Ci.nsIWritablePropertyBag2);
          prompt.setPropertyAsBool("allowTabModal", true);
          prompt.alert(title, text);
        });
    }

    if (target.classList.contains("search-one-offs-context-open-in-new-tab")) {
      // Select the context-clicked button so that consumers can easily
      // tell which button was acted on.
      this.selectedButton = this._buttonForEngine(this._contextEngine);
      this.handleSearchCommand(event, this._contextEngine, true);
    }

    if (target.classList.contains("search-one-offs-context-set-default")) {
      let currentEngine = Services.search.defaultEngine;

      if (!this.getAttribute("includecurrentengine")) {
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

      Services.search.defaultEngine = this._contextEngine;
    }
  }

  _on_contextmenu(event) {
    let target = event.originalTarget;
    // Prevent the context menu from appearing except on the one off buttons.
    if (!target.classList.contains("searchbar-engine-one-off-item") ||
        target.classList.contains("search-setting-button-compact") ||
        target.classList.contains("dummy")) {
      event.preventDefault();
      return;
    }
    this.querySelector(".search-one-offs-context-set-default")
        .setAttribute("disabled", target.engine == Services.search.defaultEngine);

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
    Services.tm.dispatchToMainThread(() => {
      this.selectedButton = null;
      this._contextEngine = null;
    });
  }
}

window.SearchOneOffs = SearchOneOffs;
