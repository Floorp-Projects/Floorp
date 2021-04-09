/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from extensionControlled.js */
/* import-globals-from preferences.js */

ChromeUtils.defineModuleGetter(
  this,
  "PlacesUtils",
  "resource://gre/modules/PlacesUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "ExtensionSettingsStore",
  "resource://gre/modules/ExtensionSettingsStore.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "UrlbarPrefs",
  "resource:///modules/UrlbarPrefs.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "UrlbarUtils",
  "resource:///modules/UrlbarUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "NimbusFeatures",
  "resource://nimbus/ExperimentAPI.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  UrlbarProviderQuickSuggest:
    "resource:///modules/UrlbarProviderQuickSuggest.jsm",
});

Preferences.addAll([
  { id: "browser.search.suggest.enabled", type: "bool" },
  { id: "browser.urlbar.suggest.searches", type: "bool" },
  { id: "browser.search.suggest.enabled.private", type: "bool" },
  { id: "browser.urlbar.suggest.quicksuggest", type: "bool" },
  { id: "browser.search.hiddenOneOffs", type: "unichar" },
  { id: "browser.search.widget.inNavBar", type: "bool" },
  { id: "browser.urlbar.showSearchSuggestionsFirst", type: "bool" },
  { id: "browser.search.separatePrivateDefault", type: "bool" },
  { id: "browser.search.separatePrivateDefault.ui.enabled", type: "bool" },
]);

const ENGINE_FLAVOR = "text/x-moz-search-engine";
const SEARCH_TYPE = "default_search";
const SEARCH_KEY = "defaultSearch";

var gEngineView = null;

var gSearchPane = {
  /**
   * Initialize autocomplete to ensure prefs are in sync.
   */
  _initAutocomplete() {
    Cc["@mozilla.org/autocomplete/search;1?name=unifiedcomplete"].getService(
      Ci.mozIPlacesAutoComplete
    );
  },

  init() {
    gEngineView = new EngineView(new EngineStore());
    document.getElementById("engineList").view = gEngineView;
    this.buildDefaultEngineDropDowns().catch(console.error);

    if (
      Services.policies &&
      !Services.policies.isAllowed("installSearchEngine")
    ) {
      document.getElementById("addEnginesBox").hidden = true;
    } else {
      let addEnginesLink = document.getElementById("addEngines");
      let searchEnginesURL = Services.wm.getMostRecentWindow(
        "navigator:browser"
      ).BrowserSearch.searchEnginesURL;
      addEnginesLink.setAttribute("href", searchEnginesURL);
    }

    window.addEventListener("click", this);
    window.addEventListener("command", this);
    window.addEventListener("dragstart", this);
    window.addEventListener("keypress", this);
    window.addEventListener("select", this);
    window.addEventListener("dblclick", this);

    Services.obs.addObserver(this, "browser-search-engine-modified");
    window.addEventListener("unload", () => {
      Services.obs.removeObserver(this, "browser-search-engine-modified");
    });

    this._initAutocomplete();

    let suggestsPref = Preferences.get("browser.search.suggest.enabled");
    let urlbarSuggestsPref = Preferences.get("browser.urlbar.suggest.searches");
    let privateSuggestsPref = Preferences.get(
      "browser.search.suggest.enabled.private"
    );
    let updateSuggestionCheckboxes = this._updateSuggestionCheckboxes.bind(
      this
    );
    suggestsPref.on("change", updateSuggestionCheckboxes);
    urlbarSuggestsPref.on("change", updateSuggestionCheckboxes);
    let urlbarSuggests = document.getElementById("urlBarSuggestion");
    urlbarSuggests.addEventListener("command", () => {
      urlbarSuggestsPref.value = urlbarSuggests.checked;
    });
    let privateWindowCheckbox = document.getElementById(
      "showSearchSuggestionsPrivateWindows"
    );
    privateWindowCheckbox.addEventListener("command", () => {
      privateSuggestsPref.value = privateWindowCheckbox.checked;
    });

    setEventListener(
      "browserSeparateDefaultEngine",
      "command",
      this._onBrowserSeparateDefaultEngineChange.bind(this)
    );
    setEventListener("openLocationBarPrivacyPreferences", "click", function(
      event
    ) {
      if (event.button == 0) {
        gotoPref("privacy-locationBar");
      }
    });

    this._initDefaultEngines();
    this._updateSuggestionCheckboxes();
    this._showAddEngineButton();
    this._updateQuickSuggest = this._updateQuickSuggest.bind(this);
    NimbusFeatures.urlbar.onUpdate(this._updateQuickSuggest);
    window.addEventListener("unload", () => {
      NimbusFeatures.urlbar.off(this._updateQuickSuggest);
    });
    this._updateQuickSuggest(true);
  },

  /**
   * Initialize the default engine handling. This will hide the private default
   * options if they are not enabled yet.
   */
  _initDefaultEngines() {
    this._separatePrivateDefaultEnabledPref = Preferences.get(
      "browser.search.separatePrivateDefault.ui.enabled"
    );

    this._separatePrivateDefaultPref = Preferences.get(
      "browser.search.separatePrivateDefault"
    );

    const checkbox = document.getElementById("browserSeparateDefaultEngine");
    checkbox.checked = !this._separatePrivateDefaultPref.value;

    this._updatePrivateEngineDisplayBoxes();

    const listener = () => {
      this._updatePrivateEngineDisplayBoxes();
      this.buildDefaultEngineDropDowns().catch(console.error);
    };

    this._separatePrivateDefaultEnabledPref.on("change", listener);
    this._separatePrivateDefaultPref.on("change", listener);
  },

  _updatePrivateEngineDisplayBoxes() {
    const separateEnabled = this._separatePrivateDefaultEnabledPref.value;
    document.getElementById(
      "browserSeparateDefaultEngine"
    ).hidden = !separateEnabled;

    const separateDefault = this._separatePrivateDefaultPref.value;

    const vbox = document.getElementById("browserPrivateEngineSelection");
    vbox.hidden = !separateEnabled || !separateDefault;
  },

  _onBrowserSeparateDefaultEngineChange(event) {
    this._separatePrivateDefaultPref.value = !event.target.checked;
  },

  _updateSuggestionCheckboxes() {
    let suggestsPref = Preferences.get("browser.search.suggest.enabled");
    let permanentPB = Services.prefs.getBoolPref(
      "browser.privatebrowsing.autostart"
    );
    let urlbarSuggests = document.getElementById("urlBarSuggestion");
    let positionCheckbox = document.getElementById(
      "showSearchSuggestionsFirstCheckbox"
    );
    let privateWindowCheckbox = document.getElementById(
      "showSearchSuggestionsPrivateWindows"
    );
    let quickSuggestCheckbox = document.getElementById("showQuickSuggest");

    urlbarSuggests.disabled = !suggestsPref.value || permanentPB;
    privateWindowCheckbox.disabled = !suggestsPref.value;
    privateWindowCheckbox.checked = Preferences.get(
      "browser.search.suggest.enabled.private"
    ).value;
    if (privateWindowCheckbox.disabled) {
      privateWindowCheckbox.checked = false;
    }

    let urlbarSuggestsPref = Preferences.get("browser.urlbar.suggest.searches");
    urlbarSuggests.checked = urlbarSuggestsPref.value;
    if (urlbarSuggests.disabled) {
      urlbarSuggests.checked = false;
    }

    if (urlbarSuggests.checked) {
      positionCheckbox.disabled = false;
      // Update the checked state of the show-suggestions-first checkbox.  Note
      // that this does *not* also update its pref, it only checks the box.
      positionCheckbox.checked = Preferences.get(
        positionCheckbox.getAttribute("preference")
      ).value;
      quickSuggestCheckbox.disabled = false;
      quickSuggestCheckbox.checked = Preferences.get(
        quickSuggestCheckbox.getAttribute("preference")
      ).value;
    } else {
      positionCheckbox.disabled = true;
      positionCheckbox.checked = false;
      quickSuggestCheckbox.disabled = true;
      quickSuggestCheckbox.checked = false;
    }

    let permanentPBLabel = document.getElementById(
      "urlBarSuggestionPermanentPBLabel"
    );
    permanentPBLabel.hidden = urlbarSuggests.hidden || !permanentPB;
  },

  /**
   * Shows or hides the Quick Suggest checkbox depending on whether the en-US
   * Quick Suggest experiment is enabled.
   *
   * @param {boolean} [onStartup]
   *   True when this is called from `.init`
   */
  _updateQuickSuggest(onStartup = false) {
    let container = document.getElementById("showQuickSuggestContainer");
    let desc = document.getElementById("searchSuggestionsDesc");

    if (!NimbusFeatures.urlbar.getValue().quickSuggestEnabled) {
      // The experiment is not enabled.  This is the default, so to avoid
      // accidentally messing anything up, only modify the doc if we're being
      // called due to a change in the experiment enabled status.
      if (!onStartup) {
        container.setAttribute("hidden", "true");
        if (desc.dataset.l10nIdOriginal) {
          desc.dataset.l10nId = desc.dataset.l10nIdOriginal;
          delete desc.dataset.l10nIdOriginal;
        }
        document.l10n.translateElements([desc]);
      }
      return;
    }

    // The experiment is enabled.
    document
      .getElementById("showQuickSuggestLearnMore")
      .setAttribute("href", UrlbarProviderQuickSuggest.helpUrl);
    container.removeAttribute("hidden");
    if (desc.dataset.l10nId) {
      desc.dataset.l10nIdOriginal = desc.dataset.l10nId;
      delete desc.dataset.l10nId;
    }
    desc.textContent = "Choose how search suggestions appear.";
  },

  _showAddEngineButton() {
    let aliasRefresh = Services.prefs.getBoolPref(
      "browser.urlbar.update2.engineAliasRefresh",
      false
    );
    if (aliasRefresh) {
      let addButton = document.getElementById("addEngineButton");
      addButton.hidden = false;
    }
  },

  /**
   * Builds the default and private engines drop down lists. This is called
   * each time something affects the list of engines.
   */
  async buildDefaultEngineDropDowns() {
    await this._buildEngineDropDown(
      document.getElementById("defaultEngine"),
      (await Services.search.getDefault()).name,
      false
    );

    if (this._separatePrivateDefaultEnabledPref.value) {
      await this._buildEngineDropDown(
        document.getElementById("defaultPrivateEngine"),
        (await Services.search.getDefaultPrivate()).name,
        true
      );
    }
  },

  /**
   * Builds a drop down menu of search engines.
   *
   * @param {DOMMenuList} list
   *   The menu list element to attach the list of engines.
   * @param {string} currentEngine
   *   The name of the current default engine.
   * @param {boolean} isPrivate
   *   True if we are dealing with the default engine for private mode.
   */
  async _buildEngineDropDown(list, currentEngine, isPrivate) {
    // If the current engine isn't in the list any more, select the first item.
    let engines = gEngineView._engineStore._engines;
    if (!engines.length) {
      return;
    }
    if (!engines.some(e => e.name == currentEngine)) {
      currentEngine = engines[0].name;
    }

    // Now clean-up and rebuild the list.
    list.removeAllItems();
    gEngineView._engineStore._engines.forEach(e => {
      let item = list.appendItem(e.name);
      item.setAttribute(
        "class",
        "menuitem-iconic searchengine-menuitem menuitem-with-favicon"
      );
      if (e.iconURI) {
        item.setAttribute("image", e.iconURI.spec);
      }
      item.engine = e;
      if (e.name == currentEngine) {
        list.selectedItem = item;
      }
    });
  },

  handleEvent(aEvent) {
    switch (aEvent.type) {
      case "dblclick":
        if (aEvent.target.id == "engineChildren") {
          let cell = aEvent.target.parentNode.getCellAt(
            aEvent.clientX,
            aEvent.clientY
          );
          if (cell.col?.id == "engineKeyword") {
            this.startEditingAlias(gEngineView.selectedIndex);
          }
        }
        break;
      case "click":
        if (
          aEvent.target.id != "engineChildren" &&
          !aEvent.target.classList.contains("searchEngineAction")
        ) {
          let engineList = document.getElementById("engineList");
          // We don't want to toggle off selection while editing keyword
          // so proceed only when the input field is hidden.
          // We need to check that engineList.view is defined here
          // because the "click" event listener is on <window> and the
          // view might have been destroyed if the pane has been navigated
          // away from.
          if (engineList.inputField.hidden && engineList.view) {
            let selection = engineList.view.selection;
            if (selection.count > 0) {
              selection.toggleSelect(selection.currentIndex);
            }
            engineList.blur();
          }
        }
        break;
      case "command":
        switch (aEvent.target.id) {
          case "":
            if (
              aEvent.target.parentNode &&
              aEvent.target.parentNode.parentNode
            ) {
              if (aEvent.target.parentNode.parentNode.id == "defaultEngine") {
                gSearchPane.setDefaultEngine();
              } else if (
                aEvent.target.parentNode.parentNode.id == "defaultPrivateEngine"
              ) {
                gSearchPane.setDefaultPrivateEngine();
              }
            }
            break;
          case "restoreDefaultSearchEngines":
            gSearchPane.onRestoreDefaults();
            break;
          case "removeEngineButton":
            Services.search.removeEngine(
              gEngineView.selectedEngine.originalEngine
            );
            break;
          case "addEngineButton":
            gSubDialog.open(
              "chrome://browser/content/preferences/dialogs/addEngine.xhtml",
              { features: "resizable=no, modal=yes" }
            );
            break;
        }
        break;
      case "dragstart":
        if (aEvent.target.id == "engineChildren") {
          onDragEngineStart(aEvent);
        }
        break;
      case "keypress":
        if (aEvent.target.id == "engineList") {
          gSearchPane.onTreeKeyPress(aEvent);
        }
        break;
      case "select":
        if (aEvent.target.id == "engineList") {
          gSearchPane.onTreeSelect();
        }
        break;
    }
  },

  /**
   * nsIObserver implementation.  We observe the following:
   *
   * * browser-search-engine-modified: Update the default engine UI and engine
   *   tree view as appropriate when engine changes occur.
   */
  observe(subject, topic, data) {
    if (topic == "browser-search-engine-modified") {
      let engine = subject;
      engine.QueryInterface(Ci.nsISearchEngine);
      switch (data) {
        case "engine-added":
          gEngineView._engineStore.addEngine(engine);
          gEngineView.rowCountChanged(gEngineView.lastEngineIndex, 1);
          gSearchPane.buildDefaultEngineDropDowns();
          break;
        case "engine-changed":
          gSearchPane.buildDefaultEngineDropDowns();
          gEngineView._engineStore.updateEngine(engine);
          gEngineView.invalidate();
          break;
        case "engine-removed":
          gSearchPane.remove(engine);
          break;
        case "engine-default": {
          // If the user is going through the drop down using up/down keys, the
          // dropdown may still be open (eg. on Windows) when engine-default is
          // fired, so rebuilding the list unconditionally would get in the way.
          let selectedEngine = document.getElementById("defaultEngine")
            .selectedItem.engine;
          if (selectedEngine.name != engine.name) {
            gSearchPane.buildDefaultEngineDropDowns();
          }
          break;
        }
        case "engine-default-private": {
          if (
            this._separatePrivateDefaultEnabledPref.value &&
            this._separatePrivateDefaultPref.value
          ) {
            // If the user is going through the drop down using up/down keys, the
            // dropdown may still be open (eg. on Windows) when engine-default is
            // fired, so rebuilding the list unconditionally would get in the way.
            const selectedEngine = document.getElementById(
              "defaultPrivateEngine"
            ).selectedItem.engine;
            if (selectedEngine.name != engine.name) {
              gSearchPane.buildDefaultEngineDropDowns();
            }
          }
          break;
        }
      }
    }
  },

  onTreeSelect() {
    document.getElementById(
      "removeEngineButton"
    ).disabled = !gEngineView.isEngineSelectedAndRemovable();
  },

  onTreeKeyPress(aEvent) {
    let index = gEngineView.selectedIndex;
    let tree = document.getElementById("engineList");
    if (tree.hasAttribute("editing")) {
      return;
    }

    if (aEvent.charCode == KeyEvent.DOM_VK_SPACE) {
      // Space toggles the checkbox.
      let newValue = !gEngineView.getCellValue(
        index,
        tree.columns.getNamedColumn("engineShown")
      );
      gEngineView.setCellValue(
        index,
        tree.columns.getFirstColumn(),
        newValue.toString()
      );
      // Prevent page from scrolling on the space key.
      aEvent.preventDefault();
    } else {
      let isMac = Services.appinfo.OS == "Darwin";
      if (
        (isMac && aEvent.keyCode == KeyEvent.DOM_VK_RETURN) ||
        (!isMac && aEvent.keyCode == KeyEvent.DOM_VK_F2)
      ) {
        this.startEditingAlias(index);
      } else if (
        aEvent.keyCode == KeyEvent.DOM_VK_DELETE ||
        (isMac &&
          aEvent.shiftKey &&
          aEvent.keyCode == KeyEvent.DOM_VK_BACK_SPACE &&
          gEngineView.isEngineSelectedAndRemovable())
      ) {
        // Delete and Shift+Backspace (Mac) removes selected engine.
        Services.search.removeEngine(gEngineView.selectedEngine.originalEngine);
      }
    }
  },

  startEditingAlias(index) {
    // Local shortcut aliases can't be edited.
    if (gEngineView._getLocalShortcut(index)) {
      return;
    }

    let tree = document.getElementById("engineList");
    let engine = gEngineView._engineStore.engines[index];
    tree.startEditing(index, tree.columns.getLastColumn());
    tree.inputField.value = engine.alias || "";
    tree.inputField.select();
  },

  async onRestoreDefaults() {
    let num = await gEngineView._engineStore.restoreDefaultEngines();
    gEngineView.rowCountChanged(0, num);
    gEngineView.invalidate();
  },

  showRestoreDefaults(aEnable) {
    document.getElementById("restoreDefaultSearchEngines").disabled = !aEnable;
  },

  remove(aEngine) {
    let index = gEngineView._engineStore.removeEngine(aEngine);
    gEngineView.rowCountChanged(index, -1);
    gEngineView.invalidate();
    gEngineView.selection.select(Math.min(index, gEngineView.rowCount - 1));
    gEngineView.ensureRowIsVisible(gEngineView.currentIndex);
    document.getElementById("engineList").focus();
  },

  async editKeyword(aEngine, aNewKeyword) {
    let keyword = aNewKeyword.trim();
    if (keyword) {
      let eduplicate = false;
      let dupName = "";

      // Check for duplicates in Places keywords.
      let bduplicate = !!(await PlacesUtils.keywords.fetch(keyword));

      // Check for duplicates in changes we haven't committed yet
      let engines = gEngineView._engineStore.engines;
      let lc_keyword = keyword.toLocaleLowerCase();
      for (let engine of engines) {
        if (
          engine.alias &&
          engine.alias.toLocaleLowerCase() == lc_keyword &&
          engine.name != aEngine.name
        ) {
          eduplicate = true;
          dupName = engine.name;
          break;
        }
      }

      // Notify the user if they have chosen an existing engine/bookmark keyword
      if (eduplicate || bduplicate) {
        let msgids = [{ id: "search-keyword-warning-title" }];
        if (eduplicate) {
          msgids.push({
            id: "search-keyword-warning-engine",
            args: { name: dupName },
          });
        } else {
          msgids.push({ id: "search-keyword-warning-bookmark" });
        }

        let [dtitle, msg] = await document.l10n.formatValues(msgids);

        Services.prompt.alert(window, dtitle, msg);
        return false;
      }
    }

    gEngineView._engineStore.changeEngine(aEngine, "alias", keyword);
    gEngineView.invalidate();
    return true;
  },

  saveOneClickEnginesList() {
    let hiddenList = [];
    for (let engine of gEngineView._engineStore.engines) {
      if (!engine.shown) {
        hiddenList.push(engine.name);
      }
    }
    Preferences.get("browser.search.hiddenOneOffs").value = hiddenList.join(
      ","
    );
  },

  async setDefaultEngine() {
    await Services.search.setDefault(
      document.getElementById("defaultEngine").selectedItem.engine
    );
    if (ExtensionSettingsStore.getSetting(SEARCH_TYPE, SEARCH_KEY) !== null) {
      ExtensionSettingsStore.select(
        ExtensionSettingsStore.SETTING_USER_SET,
        SEARCH_TYPE,
        SEARCH_KEY
      );
    }
  },

  async setDefaultPrivateEngine() {
    await Services.search.setDefaultPrivate(
      document.getElementById("defaultPrivateEngine").selectedItem.engine
    );
  },
};

function onDragEngineStart(event) {
  var selectedIndex = gEngineView.selectedIndex;

  // Local shortcut rows can't be dragged or re-ordered.
  if (gEngineView._getLocalShortcut(selectedIndex)) {
    event.preventDefault();
    return;
  }

  var tree = document.getElementById("engineList");
  let cell = tree.getCellAt(event.clientX, event.clientY);
  if (selectedIndex >= 0 && !gEngineView.isCheckBox(cell.row, cell.col)) {
    event.dataTransfer.setData(ENGINE_FLAVOR, selectedIndex.toString());
    event.dataTransfer.effectAllowed = "move";
  }
}

function EngineStore() {
  let pref = Preferences.get("browser.search.hiddenOneOffs").value;
  this.hiddenList = pref ? pref.split(",") : [];

  this._engines = [];
  this._defaultEngines = [];
  Promise.all([
    Services.search.getVisibleEngines(),
    Services.search.getAppProvidedEngines(),
  ]).then(([visibleEngines, defaultEngines]) => {
    for (let engine of visibleEngines) {
      this.addEngine(engine);
      gEngineView.rowCountChanged(gEngineView.lastEngineIndex, 1);
    }
    this._defaultEngines = defaultEngines.map(this._cloneEngine, this);
    gSearchPane.buildDefaultEngineDropDowns();

    // check if we need to disable the restore defaults button
    var someHidden = this._defaultEngines.some(e => e.hidden);
    gSearchPane.showRestoreDefaults(someHidden);
  });
}
EngineStore.prototype = {
  _engines: null,
  _defaultEngines: null,

  get engines() {
    return this._engines;
  },
  set engines(val) {
    this._engines = val;
  },

  _getIndexForEngine(aEngine) {
    return this._engines.indexOf(aEngine);
  },

  _getEngineByName(aName) {
    return this._engines.find(engine => engine.name == aName);
  },

  _cloneEngine(aEngine) {
    var clonedObj = {};
    for (let i of ["name", "alias", "iconURI", "hidden"]) {
      clonedObj[i] = aEngine[i];
    }
    clonedObj.originalEngine = aEngine;
    clonedObj.shown = !this.hiddenList.includes(clonedObj.name);
    return clonedObj;
  },

  // Callback for Array's some(). A thisObj must be passed to some()
  _isSameEngine(aEngineClone) {
    return aEngineClone.originalEngine == this.originalEngine;
  },

  addEngine(aEngine) {
    this._engines.push(this._cloneEngine(aEngine));
  },

  updateEngine(newEngine) {
    let engineToUpdate = this._engines.findIndex(
      e => e.originalEngine == newEngine
    );
    if (engineToUpdate == -1) {
      console.error("Could not find engine to update");
      return;
    }

    this.engines[engineToUpdate] = this._cloneEngine(newEngine);
  },

  moveEngine(aEngine, aNewIndex) {
    if (aNewIndex < 0 || aNewIndex > this._engines.length - 1) {
      throw new Error("ES_moveEngine: invalid aNewIndex!");
    }
    var index = this._getIndexForEngine(aEngine);
    if (index == -1) {
      throw new Error("ES_moveEngine: invalid engine?");
    }

    if (index == aNewIndex) {
      return Promise.resolve();
    } // nothing to do

    // Move the engine in our internal store
    var removedEngine = this._engines.splice(index, 1)[0];
    this._engines.splice(aNewIndex, 0, removedEngine);

    return Services.search.moveEngine(aEngine.originalEngine, aNewIndex);
  },

  removeEngine(aEngine) {
    if (this._engines.length == 1) {
      throw new Error("Cannot remove last engine!");
    }

    let engineName = aEngine.name;
    let index = this._engines.findIndex(element => element.name == engineName);

    if (index == -1) {
      throw new Error("invalid engine?");
    }

    this._engines.splice(index, 1)[0];

    if (aEngine.isAppProvided) {
      gSearchPane.showRestoreDefaults(true);
    }
    gSearchPane.buildDefaultEngineDropDowns();
    return index;
  },

  async restoreDefaultEngines() {
    var added = 0;

    for (var i = 0; i < this._defaultEngines.length; ++i) {
      var e = this._defaultEngines[i];

      // If the engine is already in the list, just move it.
      if (this._engines.some(this._isSameEngine, e)) {
        await this.moveEngine(this._getEngineByName(e.name), i);
      } else {
        // Otherwise, add it back to our internal store

        // The search service removes the alias when an engine is hidden,
        // so clear any alias we may have cached before unhiding the engine.
        e.alias = "";

        this._engines.splice(i, 0, e);
        let engine = e.originalEngine;
        engine.hidden = false;
        await Services.search.moveEngine(engine, i);
        added++;
      }
    }
    Services.search.resetToOriginalDefaultEngine();
    gSearchPane.showRestoreDefaults(false);
    gSearchPane.buildDefaultEngineDropDowns();
    return added;
  },

  changeEngine(aEngine, aProp, aNewValue) {
    var index = this._getIndexForEngine(aEngine);
    if (index == -1) {
      throw new Error("invalid engine?");
    }

    this._engines[index][aProp] = aNewValue;
    aEngine.originalEngine[aProp] = aNewValue;
  },

  reloadIcons() {
    this._engines.forEach(function(e) {
      e.iconURI = e.originalEngine.iconURI;
    });
  },
};

function EngineView(aEngineStore) {
  this._engineStore = aEngineStore;

  UrlbarPrefs.addObserver(this);

  // This maps local shortcut sources to their l10n names.  The names are needed
  // by getCellText.  Getting the names is async but getCellText is not, so we
  // cache them here to retrieve them syncronously in getCellText.
  this._localShortcutL10nNames = new Map();
  document.l10n
    .formatValues(
      UrlbarUtils.LOCAL_SEARCH_MODES.map(mode => {
        let name = UrlbarUtils.getResultSourceName(mode.source);
        return { id: `urlbar-search-mode-${name}` };
      })
    )
    .then(names => {
      for (let { source } of UrlbarUtils.LOCAL_SEARCH_MODES) {
        this._localShortcutL10nNames.set(source, names.shift());
      }
      // Invalidate the tree now that we have the names in case getCellText was
      // called before name retrieval finished.
      this.invalidate();
    });
}

EngineView.prototype = {
  _engineStore: null,
  tree: null,

  get lastEngineIndex() {
    return this._engineStore.engines.length - 1;
  },

  get selectedIndex() {
    var seln = this.selection;
    if (seln.getRangeCount() > 0) {
      var min = {};
      seln.getRangeAt(0, min, {});
      return min.value;
    }
    return -1;
  },

  get selectedEngine() {
    return this._engineStore.engines[this.selectedIndex];
  },

  // Helpers
  rowCountChanged(index, count) {
    if (this.tree) {
      this.tree.rowCountChanged(index, count);
    }
  },

  invalidate() {
    this.tree?.invalidate();
  },

  ensureRowIsVisible(index) {
    this.tree.ensureRowIsVisible(index);
  },

  getSourceIndexFromDrag(dataTransfer) {
    return parseInt(dataTransfer.getData(ENGINE_FLAVOR));
  },

  isCheckBox(index, column) {
    return column.id == "engineShown";
  },

  isEngineSelectedAndRemovable() {
    // We don't allow the last remaining engine to be removed, thus the
    // `this.lastEngineIndex != 0` check.
    return (
      this.selectedIndex != -1 &&
      this.lastEngineIndex != 0 &&
      !this._getLocalShortcut(this.selectedIndex)
    );
  },

  /**
   * Returns the local shortcut corresponding to a tree row, or null if the row
   * is not a local shortcut.
   *
   * @param {number} index
   *   The tree row index.
   * @returns {object}
   *   The local shortcut object or null if the row is not a local shortcut.
   */
  _getLocalShortcut(index) {
    let engineCount = this._engineStore.engines.length;
    if (index < engineCount) {
      return null;
    }
    return UrlbarUtils.LOCAL_SEARCH_MODES[index - engineCount];
  },

  /**
   * Called by UrlbarPrefs when a urlbar pref changes.
   *
   * @param {string} pref
   *   The name of the pref relative to the browser.urlbar branch.
   */
  onPrefChanged(pref) {
    // If one of the local shortcut prefs was toggled, toggle its row's
    // checkbox.
    let parts = pref.split(".");
    if (parts[0] == "shortcuts" && parts[1] && parts.length == 2) {
      this.invalidate();
    }
  },

  // nsITreeView
  get rowCount() {
    return (
      this._engineStore.engines.length + UrlbarUtils.LOCAL_SEARCH_MODES.length
    );
  },

  getImageSrc(index, column) {
    if (column.id == "engineName") {
      let shortcut = this._getLocalShortcut(index);
      if (shortcut) {
        return shortcut.icon;
      }

      if (this._engineStore.engines[index].iconURI) {
        return this._engineStore.engines[index].iconURI.spec;
      }

      if (window.devicePixelRatio > 1) {
        return "chrome://browser/skin/search-engine-placeholder@2x.png";
      }
      return "chrome://browser/skin/search-engine-placeholder.png";
    }

    return "";
  },

  getCellText(index, column) {
    if (column.id == "engineName") {
      let shortcut = this._getLocalShortcut(index);
      if (shortcut) {
        return this._localShortcutL10nNames.get(shortcut.source) || "";
      }
      return this._engineStore.engines[index].name;
    } else if (column.id == "engineKeyword") {
      let shortcut = this._getLocalShortcut(index);
      if (shortcut) {
        return shortcut.restrict;
      }
      return this._engineStore.engines[index].originalEngine.aliases.join(", ");
    }
    return "";
  },

  setTree(tree) {
    this.tree = tree;
  },

  canDrop(targetIndex, orientation, dataTransfer) {
    var sourceIndex = this.getSourceIndexFromDrag(dataTransfer);
    return (
      sourceIndex != -1 &&
      sourceIndex != targetIndex &&
      sourceIndex != targetIndex + orientation &&
      // Local shortcut rows can't be dragged or dropped on.
      targetIndex < this._engineStore.engines.length
    );
  },

  async drop(dropIndex, orientation, dataTransfer) {
    // Local shortcut rows can't be dragged or dropped on.  This can sometimes
    // be reached even though canDrop returns false for these rows.
    if (this._engineStore.engines.length <= dropIndex) {
      return;
    }

    var sourceIndex = this.getSourceIndexFromDrag(dataTransfer);
    var sourceEngine = this._engineStore.engines[sourceIndex];

    const nsITreeView = Ci.nsITreeView;
    if (dropIndex > sourceIndex) {
      if (orientation == nsITreeView.DROP_BEFORE) {
        dropIndex--;
      }
    } else if (orientation == nsITreeView.DROP_AFTER) {
      dropIndex++;
    }

    await this._engineStore.moveEngine(sourceEngine, dropIndex);
    gSearchPane.showRestoreDefaults(true);
    gSearchPane.buildDefaultEngineDropDowns();

    // Redraw, and adjust selection
    this.invalidate();
    this.selection.select(dropIndex);
  },

  selection: null,
  getRowProperties(index) {
    return "";
  },
  getCellProperties(index, column) {
    if (column.id == "engineName") {
      // For local shortcut rows, return the result source name so we can style
      // the icons in CSS.
      let shortcut = this._getLocalShortcut(index);
      if (shortcut) {
        return UrlbarUtils.getResultSourceName(shortcut.source);
      }
    }
    return "";
  },
  getColumnProperties(column) {
    return "";
  },
  isContainer(index) {
    return false;
  },
  isContainerOpen(index) {
    return false;
  },
  isContainerEmpty(index) {
    return false;
  },
  isSeparator(index) {
    return false;
  },
  isSorted(index) {
    return false;
  },
  getParentIndex(index) {
    return -1;
  },
  hasNextSibling(parentIndex, index) {
    return false;
  },
  getLevel(index) {
    return 0;
  },
  getCellValue(index, column) {
    if (column.id == "engineShown") {
      let shortcut = this._getLocalShortcut(index);
      if (shortcut) {
        return UrlbarPrefs.get(shortcut.pref);
      }
      return this._engineStore.engines[index].shown;
    }
    return undefined;
  },
  toggleOpenState(index) {},
  cycleHeader(column) {},
  selectionChanged() {},
  cycleCell(row, column) {},
  isEditable(index, column) {
    return (
      column.id != "engineName" &&
      (column.id == "engineShown" || !this._getLocalShortcut(index))
    );
  },
  setCellValue(index, column, value) {
    if (column.id == "engineShown") {
      let shortcut = this._getLocalShortcut(index);
      if (shortcut) {
        UrlbarPrefs.set(shortcut.pref, value == "true");
        this.invalidate();
        return;
      }
      this._engineStore.engines[index].shown = value == "true";
      gEngineView.invalidate();
      gSearchPane.saveOneClickEnginesList();
    }
  },
  setCellText(index, column, value) {
    if (column.id == "engineKeyword") {
      gSearchPane
        .editKeyword(this._engineStore.engines[index], value)
        .then(valid => {
          if (!valid) {
            gSearchPane.startEditingAlias(index);
          }
        });
    }
  },
};
