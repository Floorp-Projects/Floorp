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

Preferences.addAll([
  { id: "browser.search.suggest.enabled", type: "bool" },
  { id: "browser.urlbar.suggest.searches", type: "bool" },
  { id: "browser.search.hiddenOneOffs", type: "unichar" },
  { id: "browser.search.widget.inNavBar", type: "bool" },
  { id: "browser.urlbar.matchBuckets", type: "string" },
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
    this.buildDefaultEngineDropDown();

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
    window.addEventListener("blur", this, true);

    Services.obs.addObserver(this, "browser-search-engine-modified");
    window.addEventListener("unload", () => {
      Services.obs.removeObserver(this, "browser-search-engine-modified");
    });

    this._initAutocomplete();

    let suggestsPref = Preferences.get("browser.search.suggest.enabled");
    let urlbarSuggestsPref = Preferences.get("browser.urlbar.suggest.searches");
    let updateSuggestionCheckboxes = this._updateSuggestionCheckboxes.bind(
      this
    );
    suggestsPref.on("change", updateSuggestionCheckboxes);
    urlbarSuggestsPref.on("change", updateSuggestionCheckboxes);
    let urlbarSuggests = document.getElementById("urlBarSuggestion");
    urlbarSuggests.addEventListener("command", () => {
      urlbarSuggestsPref.value = urlbarSuggests.checked;
    });

    this._initShowSearchSuggestionsFirst();
    this._updateSuggestionCheckboxes();
  },

  _initShowSearchSuggestionsFirst() {
    this._urlbarSuggestionsPosPref = Preferences.get(
      "browser.urlbar.matchBuckets"
    );
    let checkbox = document.getElementById(
      "showSearchSuggestionsFirstCheckbox"
    );

    this._urlbarSuggestionsPosPref.on("change", () => {
      this._syncFromShowSearchSuggestionsFirstPref(checkbox);
    });
    this._syncFromShowSearchSuggestionsFirstPref(checkbox);

    checkbox.addEventListener("command", () => {
      this._syncToShowSearchSuggestionsFirstPref(checkbox.checked);
    });
  },

  _syncFromShowSearchSuggestionsFirstPref(checkbox) {
    if (!this._urlbarSuggestionsPosPref.value) {
      // The pref is cleared, meaning search suggestions are shown first.
      checkbox.checked = true;
      return;
    }
    // The pref has a value.  If the first bucket in the pref is search
    // suggestions, then check the checkbox.
    let buckets = PlacesUtils.convertMatchBucketsStringToArray(
      this._urlbarSuggestionsPosPref.value
    );
    checkbox.checked = buckets[0] && buckets[0][0] == "suggestion";
  },

  _syncToShowSearchSuggestionsFirstPref(checked) {
    if (checked) {
      // Show search suggestions first, so clear the pref since that's the
      // default.
      this._urlbarSuggestionsPosPref.reset();
      return;
    }
    // Show history first.
    this._urlbarSuggestionsPosPref.value = "general:5,suggestion:Infinity";
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

    urlbarSuggests.disabled = !suggestsPref.value || permanentPB;

    let urlbarSuggestsPref = Preferences.get("browser.urlbar.suggest.searches");
    urlbarSuggests.checked = urlbarSuggestsPref.value;
    if (urlbarSuggests.disabled) {
      urlbarSuggests.checked = false;
    }

    if (urlbarSuggests.checked) {
      positionCheckbox.disabled = false;
      this._syncFromShowSearchSuggestionsFirstPref(positionCheckbox);
    } else {
      positionCheckbox.disabled = true;
      positionCheckbox.checked = false;
    }

    let permanentPBLabel = document.getElementById(
      "urlBarSuggestionPermanentPBLabel"
    );
    permanentPBLabel.hidden = urlbarSuggests.hidden || !permanentPB;
  },

  async buildDefaultEngineDropDown() {
    // This is called each time something affects the list of engines.
    let list = document.getElementById("defaultEngine");
    // Set selection to the current default engine.
    let currentEngine = (await Services.search.getDefault()).name;

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

    handleControllingExtension(SEARCH_TYPE, SEARCH_KEY);
    let searchEngineListener = {
      observe(subject, topic, data) {
        handleControllingExtension(SEARCH_TYPE, SEARCH_KEY);
      },
    };
    Services.obs.addObserver(
      searchEngineListener,
      "browser-search-engine-modified"
    );
    window.addEventListener("unload", () => {
      Services.obs.removeObserver(
        searchEngineListener,
        "browser-search-engine-modified"
      );
    });
  },

  handleEvent(aEvent) {
    switch (aEvent.type) {
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
              aEvent.target.parentNode.parentNode &&
              aEvent.target.parentNode.parentNode.id == "defaultEngine"
            ) {
              gSearchPane.setDefaultEngine();
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
      case "blur":
        if (
          aEvent.target.id == "engineList" &&
          aEvent.target.inputField ==
            document.getBindingParent(aEvent.originalTarget)
        ) {
          gSearchPane.onInputBlur(aEvent);
        }
        break;
    }
  },

  observe(aEngine, aTopic, aVerb) {
    if (aTopic == "browser-search-engine-modified") {
      aEngine.QueryInterface(Ci.nsISearchEngine);
      switch (aVerb) {
        case "engine-added":
          gEngineView._engineStore.addEngine(aEngine);
          gEngineView.rowCountChanged(gEngineView.lastIndex, 1);
          gSearchPane.buildDefaultEngineDropDown();
          break;
        case "engine-changed":
          gEngineView._engineStore.reloadIcons();
          gEngineView.invalidate();
          break;
        case "engine-removed":
          gSearchPane.remove(aEngine);
          break;
        case "engine-default":
          // If the user is going through the drop down using up/down keys, the
          // dropdown may still be open (eg. on Windows) when engine-default is
          // fired, so rebuilding the list unconditionally would get in the way.
          let selectedEngine = document.getElementById("defaultEngine")
            .selectedItem.engine;
          if (selectedEngine.name != aEngine.name) {
            gSearchPane.buildDefaultEngineDropDown();
          }
          break;
      }
    }
  },

  onInputBlur(aEvent) {
    let tree = document.getElementById("engineList");
    if (!tree.hasAttribute("editing")) {
      return;
    }

    // Accept input unless discarded.
    let accept = aEvent.charCode != KeyEvent.DOM_VK_ESCAPE;
    tree.stopEditing(accept);
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
      let newValue = !gEngineView._engineStore.engines[index].shown;
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
        tree.startEditing(index, tree.columns.getLastColumn());
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
    gEngineView.selection.select(Math.min(index, gEngineView.lastIndex));
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
    ExtensionSettingsStore.setByUser(SEARCH_TYPE, SEARCH_KEY);
  },
};

function onDragEngineStart(event) {
  var selectedIndex = gEngineView.selectedIndex;
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
    Services.search.getDefaultEngines(),
  ]).then(([visibleEngines, defaultEngines]) => {
    for (let engine of visibleEngines) {
      this.addEngine(engine);
      gEngineView.rowCountChanged(gEngineView.lastIndex, 1);
    }
    this._defaultEngines = defaultEngines.map(this._cloneEngine, this);
    gSearchPane.buildDefaultEngineDropDown();

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
    return val;
  },

  _getIndexForEngine(aEngine) {
    return this._engines.indexOf(aEngine);
  },

  _getEngineByName(aName) {
    return this._engines.find(engine => engine.name == aName);
  },

  _cloneEngine(aEngine) {
    var clonedObj = {};
    for (var i in aEngine) {
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

    let removedEngine = this._engines.splice(index, 1)[0];

    if (this._defaultEngines.some(this._isSameEngine, removedEngine)) {
      gSearchPane.showRestoreDefaults(true);
    }
    gSearchPane.buildDefaultEngineDropDown();
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
    gSearchPane.buildDefaultEngineDropDown();
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
      e.uri = e.originalEngine.uri;
    });
  },
};

function EngineView(aEngineStore) {
  this._engineStore = aEngineStore;
}
EngineView.prototype = {
  _engineStore: null,
  tree: null,

  get lastIndex() {
    return this.rowCount - 1;
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
    this.tree.invalidate();
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
    return this.selectedIndex != -1 && this.lastIndex != 0;
  },

  // nsITreeView
  get rowCount() {
    return this._engineStore.engines.length;
  },

  getImageSrc(index, column) {
    if (column.id == "engineName") {
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
      return this._engineStore.engines[index].name;
    } else if (column.id == "engineKeyword") {
      return this._engineStore.engines[index].alias;
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
      sourceIndex != targetIndex + orientation
    );
  },

  async drop(dropIndex, orientation, dataTransfer) {
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
    gSearchPane.buildDefaultEngineDropDown();

    // Redraw, and adjust selection
    this.invalidate();
    this.selection.select(dropIndex);
  },

  selection: null,
  getRowProperties(index) {
    return "";
  },
  getCellProperties(index, column) {
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
      return this._engineStore.engines[index].shown;
    }
    return undefined;
  },
  toggleOpenState(index) {},
  cycleHeader(column) {},
  selectionChanged() {},
  cycleCell(row, column) {},
  isEditable(index, column) {
    return column.id != "engineName";
  },
  setCellValue(index, column, value) {
    if (column.id == "engineShown") {
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
            document.getElementById("engineList").startEditing(index, column);
          }
        });
    }
  },
};
