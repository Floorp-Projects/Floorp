/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "AppConstants",
                                  "resource://gre/modules/AppConstants.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
                                  "resource://gre/modules/PlacesUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
                                  "resource://gre/modules/Task.jsm");

const ENGINE_FLAVOR = "text/x-moz-search-engine";

document.addEventListener("Initialized", () => {
  if (!AppConstants.isPlatformAndVersionAtLeast("win", "10")) {
    document.getElementById("redirectSearchCheckbox").hidden = true;
  }
});

var gEngineView = null;

var gSearchPane = {

  /**
   * Initialize autocomplete to ensure prefs are in sync.
   */
  _initAutocomplete: function () {
    Components.classes["@mozilla.org/autocomplete/search;1?name=unifiedcomplete"]
              .getService(Components.interfaces.mozIPlacesAutoComplete);
  },

  init: function ()
  {
    gEngineView = new EngineView(new EngineStore());
    document.getElementById("engineList").view = gEngineView;
    this.buildDefaultEngineDropDown();

    window.addEventListener("click", this, false);
    window.addEventListener("command", this, false);
    window.addEventListener("dragstart", this, false);
    window.addEventListener("keypress", this, false);
    window.addEventListener("select", this, false);
    window.addEventListener("blur", this, true);

    Services.obs.addObserver(this, "browser-search-engine-modified", false);
    window.addEventListener("unload", () => {
      Services.obs.removeObserver(this, "browser-search-engine-modified", false);
    });

    this._initAutocomplete();

    let suggestsPref =
      document.getElementById("browser.search.suggest.enabled");
    suggestsPref.addEventListener("change", () => {
      this.updateSuggestsCheckbox();
    });
    this.updateSuggestsCheckbox();
  },

  updateSuggestsCheckbox() {
    let suggestsPref =
      document.getElementById("browser.search.suggest.enabled");
    let permanentPB =
      Services.prefs.getBoolPref("browser.privatebrowsing.autostart");
    let urlbarSuggests = document.getElementById("urlBarSuggestion");
    urlbarSuggests.disabled = !suggestsPref.value || permanentPB;

    let urlbarSuggestsPref =
      document.getElementById("browser.urlbar.suggest.searches");
    urlbarSuggests.checked = urlbarSuggestsPref.value;
    if (urlbarSuggests.disabled) {
      urlbarSuggests.checked = false;
    }

    let permanentPBLabel =
      document.getElementById("urlBarSuggestionPermanentPBLabel");
    permanentPBLabel.hidden = urlbarSuggests.hidden || !permanentPB;
  },

  buildDefaultEngineDropDown: function() {
    // This is called each time something affects the list of engines.
    let list = document.getElementById("defaultEngine");
    // Set selection to the current default engine.
    let currentEngine = Services.search.currentEngine.name;

    // If the current engine isn't in the list any more, select the first item.
    let engines = gEngineView._engineStore._engines;
    if (!engines.some(e => e.name == currentEngine))
      currentEngine = engines[0].name;

    // Now clean-up and rebuild the list.
    list.removeAllItems();
    gEngineView._engineStore._engines.forEach(e => {
      let item = list.appendItem(e.name);
      item.setAttribute("class", "menuitem-iconic searchengine-menuitem menuitem-with-favicon");
      if (e.iconURI) {
        item.setAttribute("image", e.iconURI.spec);
      }
      item.engine = e;
      if (e.name == currentEngine)
        list.selectedItem = item;
    });
  },

  handleEvent: function(aEvent) {
    switch (aEvent.type) {
      case "click":
        if (aEvent.target.id != "engineChildren" &&
            !aEvent.target.classList.contains("searchEngineAction")) {
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
        if (aEvent.target.id == "addEngines" && aEvent.button == 0) {
          Services.wm.getMostRecentWindow('navigator:browser')
                     .BrowserSearch.loadAddEngines();
        }
        break;
      case "command":
        switch (aEvent.target.id) {
          case "":
            if (aEvent.target.parentNode &&
                aEvent.target.parentNode.parentNode &&
                aEvent.target.parentNode.parentNode.id == "defaultEngine") {
              gSearchPane.setDefaultEngine();
            }
            break;
          case "restoreDefaultSearchEngines":
            gSearchPane.onRestoreDefaults();
            break;
          case "removeEngineButton":
            Services.search.removeEngine(gEngineView.selectedEngine.originalEngine);
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
        if (aEvent.target.id == "engineList" &&
            aEvent.target.inputField == document.getBindingParent(aEvent.originalTarget)) {
          gSearchPane.onInputBlur();
        }
        break;
    }
  },

  observe: function(aEngine, aTopic, aVerb) {
    if (aTopic == "browser-search-engine-modified") {
      aEngine.QueryInterface(Components.interfaces.nsISearchEngine);
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
      case "engine-current":
        // If the user is going through the drop down using up/down keys, the
        // dropdown may still be open (eg. on Windows) when engine-current is
        // fired, so rebuilding the list unconditionally would get in the way.
        let selectedEngine =
          document.getElementById("defaultEngine").selectedItem.engine;
        if (selectedEngine.name != aEngine.name)
          gSearchPane.buildDefaultEngineDropDown();
        break;
      case "engine-default":
        // Not relevant
        break;
      }
    }
  },

  onInputBlur: function() {
    let tree = document.getElementById("engineList");
    tree.stopEditing(false);
  },

  onTreeSelect: function() {
    document.getElementById("removeEngineButton").disabled =
      !gEngineView.isEngineSelectedAndRemovable();
  },

  onTreeKeyPress: function(aEvent) {
    let index = gEngineView.selectedIndex;
    let tree = document.getElementById("engineList");
    if (tree.hasAttribute("editing"))
      return;

    if (aEvent.charCode == KeyEvent.DOM_VK_SPACE) {
      // Space toggles the checkbox.
      let newValue = !gEngineView._engineStore.engines[index].shown;
      gEngineView.setCellValue(index, tree.columns.getFirstColumn(),
                               newValue.toString());
    }
    else {
      let isMac = Services.appinfo.OS == "Darwin";
      if ((isMac && aEvent.keyCode == KeyEvent.DOM_VK_RETURN) ||
          (!isMac && aEvent.keyCode == KeyEvent.DOM_VK_F2)) {
        tree.startEditing(index, tree.columns.getLastColumn());
      } else if (aEvent.keyCode == KeyEvent.DOM_VK_DELETE ||
                 (isMac && aEvent.shiftKey &&
                  aEvent.keyCode == KeyEvent.DOM_VK_BACK_SPACE &&
                  gEngineView.isEngineSelectedAndRemovable())) {
        // Delete and Shift+Backspace (Mac) removes selected engine.
        Services.search.removeEngine(gEngineView.selectedEngine.originalEngine);
     }
    }
  },

  onRestoreDefaults: function() {
    let num = gEngineView._engineStore.restoreDefaultEngines();
    gEngineView.rowCountChanged(0, num);
    gEngineView.invalidate();
  },

  showRestoreDefaults: function(aEnable) {
    document.getElementById("restoreDefaultSearchEngines").disabled = !aEnable;
  },

  remove: function(aEngine) {
    let index = gEngineView._engineStore.removeEngine(aEngine);
    gEngineView.rowCountChanged(index, -1);
    gEngineView.invalidate();
    gEngineView.selection.select(Math.min(index, gEngineView.lastIndex));
    gEngineView.ensureRowIsVisible(gEngineView.currentIndex);
    document.getElementById("engineList").focus();
  },

  editKeyword: Task.async(function* (aEngine, aNewKeyword) {
    let keyword = aNewKeyword.trim();
    if (keyword) {
      let eduplicate = false;
      let dupName = "";

      // Check for duplicates in Places keywords.
      let bduplicate = !!(yield PlacesUtils.keywords.fetch(keyword));

      // Check for duplicates in changes we haven't committed yet
      let engines = gEngineView._engineStore.engines;
      for (let engine of engines) {
        if (engine.alias == keyword &&
            engine.name != aEngine.name) {
          eduplicate = true;
          dupName = engine.name;
          break;
        }
      }

      // Notify the user if they have chosen an existing engine/bookmark keyword
      if (eduplicate || bduplicate) {
        let strings = document.getElementById("engineManagerBundle");
        let dtitle = strings.getString("duplicateTitle");
        let bmsg = strings.getString("duplicateBookmarkMsg");
        let emsg = strings.getFormattedString("duplicateEngineMsg", [dupName]);

        Services.prompt.alert(window, dtitle, eduplicate ? emsg : bmsg);
        return false;
      }
    }

    gEngineView._engineStore.changeEngine(aEngine, "alias", keyword);
    gEngineView.invalidate();
    return true;
  }),

  saveOneClickEnginesList: function () {
    let hiddenList = [];
    for (let engine of gEngineView._engineStore.engines) {
      if (!engine.shown)
        hiddenList.push(engine.name);
    }
    document.getElementById("browser.search.hiddenOneOffs").value =
      hiddenList.join(",");
  },

  setDefaultEngine: function () {
    Services.search.currentEngine =
      document.getElementById("defaultEngine").selectedItem.engine;
  }
};

function onDragEngineStart(event) {
  var selectedIndex = gEngineView.selectedIndex;
  var tree = document.getElementById("engineList");
  var row = { }, col = { }, child = { };
  tree.treeBoxObject.getCellAt(event.clientX, event.clientY, row, col, child);
  if (selectedIndex >= 0 && !gEngineView.isCheckBox(row.value, col.value)) {
    event.dataTransfer.setData(ENGINE_FLAVOR, selectedIndex.toString());
    event.dataTransfer.effectAllowed = "move";
  }
}


function EngineStore() {
  let pref = document.getElementById("browser.search.hiddenOneOffs").value;
  this.hiddenList = pref ? pref.split(",") : [];

  this._engines = Services.search.getVisibleEngines().map(this._cloneEngine, this);
  this._defaultEngines = Services.search.getDefaultEngines().map(this._cloneEngine, this);

  // check if we need to disable the restore defaults button
  var someHidden = this._defaultEngines.some(e => e.hidden);
  gSearchPane.showRestoreDefaults(someHidden);
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

  _getIndexForEngine: function ES_getIndexForEngine(aEngine) {
    return this._engines.indexOf(aEngine);
  },

  _getEngineByName: function ES_getEngineByName(aName) {
    return this._engines.find(engine => engine.name == aName);
  },

  _cloneEngine: function ES_cloneEngine(aEngine) {
    var clonedObj={};
    for (var i in aEngine)
      clonedObj[i] = aEngine[i];
    clonedObj.originalEngine = aEngine;
    clonedObj.shown = this.hiddenList.indexOf(clonedObj.name) == -1;
    return clonedObj;
  },

  // Callback for Array's some(). A thisObj must be passed to some()
  _isSameEngine: function ES_isSameEngine(aEngineClone) {
    return aEngineClone.originalEngine == this.originalEngine;
  },

  addEngine: function ES_addEngine(aEngine) {
    this._engines.push(this._cloneEngine(aEngine));
  },

  moveEngine: function ES_moveEngine(aEngine, aNewIndex) {
    if (aNewIndex < 0 || aNewIndex > this._engines.length - 1)
      throw new Error("ES_moveEngine: invalid aNewIndex!");
    var index = this._getIndexForEngine(aEngine);
    if (index == -1)
      throw new Error("ES_moveEngine: invalid engine?");

    if (index == aNewIndex)
      return; // nothing to do

    // Move the engine in our internal store
    var removedEngine = this._engines.splice(index, 1)[0];
    this._engines.splice(aNewIndex, 0, removedEngine);

    Services.search.moveEngine(aEngine.originalEngine, aNewIndex);
  },

  removeEngine: function ES_removeEngine(aEngine) {
    if (this._engines.length == 1) {
      throw new Error("Cannot remove last engine!");
    }

    let engineName = aEngine.name;
    let index = this._engines.findIndex(element => element.name == engineName);

    if (index == -1)
      throw new Error("invalid engine?");

    this._engines.splice(index, 1);

    if (this._defaultEngines.some(this._isSameEngine, this._engines[index]))
      gSearchPane.showRestoreDefaults(true);
    gSearchPane.buildDefaultEngineDropDown();
    return index;
  },

  restoreDefaultEngines: function ES_restoreDefaultEngines() {
    var added = 0;

    for (var i = 0; i < this._defaultEngines.length; ++i) {
      var e = this._defaultEngines[i];

      // If the engine is already in the list, just move it.
      if (this._engines.some(this._isSameEngine, e)) {
        this.moveEngine(this._getEngineByName(e.name), i);
      } else {
        // Otherwise, add it back to our internal store

        // The search service removes the alias when an engine is hidden,
        // so clear any alias we may have cached before unhiding the engine.
        e.alias = "";

        this._engines.splice(i, 0, e);
        let engine = e.originalEngine;
        engine.hidden = false;
        Services.search.moveEngine(engine, i);
        added++;
      }
    }
    Services.search.resetToOriginalDefaultEngine();
    gSearchPane.showRestoreDefaults(false);
    gSearchPane.buildDefaultEngineDropDown();
    return added;
  },

  changeEngine: function ES_changeEngine(aEngine, aProp, aNewValue) {
    var index = this._getIndexForEngine(aEngine);
    if (index == -1)
      throw new Error("invalid engine?");

    this._engines[index][aProp] = aNewValue;
    aEngine.originalEngine[aProp] = aNewValue;
  },

  reloadIcons: function ES_reloadIcons() {
    this._engines.forEach(function (e) {
      e.uri = e.originalEngine.uri;
    });
  }
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
  rowCountChanged: function (index, count) {
    this.tree.rowCountChanged(index, count);
  },

  invalidate: function () {
    this.tree.invalidate();
  },

  ensureRowIsVisible: function (index) {
    this.tree.ensureRowIsVisible(index);
  },

  getSourceIndexFromDrag: function (dataTransfer) {
    return parseInt(dataTransfer.getData(ENGINE_FLAVOR));
  },

  isCheckBox: function(index, column) {
    return column.id == "engineShown";
  },

  isEngineSelectedAndRemovable: function() {
    return this.selectedIndex != -1 && this.lastIndex != 0;
  },

  // nsITreeView
  get rowCount() {
    return this._engineStore.engines.length;
  },

  getImageSrc: function(index, column) {
    if (column.id == "engineName") {
      if (this._engineStore.engines[index].iconURI)
        return this._engineStore.engines[index].iconURI.spec;

      if (window.devicePixelRatio > 1)
        return "chrome://browser/skin/search-engine-placeholder@2x.png";
      return "chrome://browser/skin/search-engine-placeholder.png";
    }

    return "";
  },

  getCellText: function(index, column) {
    if (column.id == "engineName")
      return this._engineStore.engines[index].name;
    else if (column.id == "engineKeyword")
      return this._engineStore.engines[index].alias;
    return "";
  },

  setTree: function(tree) {
    this.tree = tree;
  },

  canDrop: function(targetIndex, orientation, dataTransfer) {
    var sourceIndex = this.getSourceIndexFromDrag(dataTransfer);
    return (sourceIndex != -1 &&
            sourceIndex != targetIndex &&
            sourceIndex != targetIndex + orientation);
  },

  drop: function(dropIndex, orientation, dataTransfer) {
    var sourceIndex = this.getSourceIndexFromDrag(dataTransfer);
    var sourceEngine = this._engineStore.engines[sourceIndex];

    const nsITreeView = Components.interfaces.nsITreeView;
    if (dropIndex > sourceIndex) {
      if (orientation == nsITreeView.DROP_BEFORE)
        dropIndex--;
    } else if (orientation == nsITreeView.DROP_AFTER) {
      dropIndex++;
    }

    this._engineStore.moveEngine(sourceEngine, dropIndex);
    gSearchPane.showRestoreDefaults(true);
    gSearchPane.buildDefaultEngineDropDown();

    // Redraw, and adjust selection
    this.invalidate();
    this.selection.select(dropIndex);
  },

  selection: null,
  getRowProperties: function(index) { return ""; },
  getCellProperties: function(index, column) { return ""; },
  getColumnProperties: function(column) { return ""; },
  isContainer: function(index) { return false; },
  isContainerOpen: function(index) { return false; },
  isContainerEmpty: function(index) { return false; },
  isSeparator: function(index) { return false; },
  isSorted: function(index) { return false; },
  getParentIndex: function(index) { return -1; },
  hasNextSibling: function(parentIndex, index) { return false; },
  getLevel: function(index) { return 0; },
  getProgressMode: function(index, column) { },
  getCellValue: function(index, column) {
    if (column.id == "engineShown")
      return this._engineStore.engines[index].shown;
    return undefined;
  },
  toggleOpenState: function(index) { },
  cycleHeader: function(column) { },
  selectionChanged: function() { },
  cycleCell: function(row, column) { },
  isEditable: function(index, column) { return column.id != "engineName"; },
  isSelectable: function(index, column) { return false; },
  setCellValue: function(index, column, value) {
    if (column.id == "engineShown") {
      this._engineStore.engines[index].shown = value == "true";
      gEngineView.invalidate();
      gSearchPane.saveOneClickEnginesList();
    }
  },
  setCellText: function(index, column, value) {
    if (column.id == "engineKeyword") {
      gSearchPane.editKeyword(this._engineStore.engines[index], value)
                 .then(valid => {
        if (!valid)
          document.getElementById("engineList").startEditing(index, column);
      });
    }
  },
  performAction: function(action) { },
  performActionOnRow: function(action, index) { },
  performActionOnCell: function(action, index, column) { }
};
