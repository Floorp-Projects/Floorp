/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/Services.jsm");

const Ci = Components.interfaces;
const Cc = Components.classes;

const ENGINE_FLAVOR = "text/x-moz-search-engine";

const BROWSER_SUGGEST_PREF = "browser.search.suggest.enabled";

var gEngineView = null;

var gEngineManagerDialog = {
  init: function engineManager_init() {
    gEngineView = new EngineView(new EngineStore());

    var suggestEnabled = Services.prefs.getBoolPref(BROWSER_SUGGEST_PREF);
    document.getElementById("enableSuggest").checked = suggestEnabled;

    var tree = document.getElementById("engineList");
    tree.view = gEngineView;

    Services.obs.addObserver(this, "browser-search-engine-modified", false);
  },

  destroy: function engineManager_destroy() {
    // Remove the observer
    Services.obs.removeObserver(this, "browser-search-engine-modified");
  },

  observe: function engineManager_observe(aEngine, aTopic, aVerb) {
    if (aTopic == "browser-search-engine-modified") {
      aEngine.QueryInterface(Ci.nsISearchEngine);
      switch (aVerb) {
      case "engine-added":
        gEngineView._engineStore.addEngine(aEngine);
        gEngineView.rowCountChanged(gEngineView.lastIndex, 1);
        break;
      case "engine-changed":
        gEngineView._engineStore.reloadIcons();
        gEngineView.invalidate();
        break;
      case "engine-removed":
      case "engine-current":
      case "engine-default":
        // Not relevant
        break;
      }
    }
  },

  onOK: function engineManager_onOK() {
    // Set the preference
    var newSuggestEnabled = document.getElementById("enableSuggest").checked;
    Services.prefs.setBoolPref(BROWSER_SUGGEST_PREF, newSuggestEnabled);

    // Commit the changes
    gEngineView._engineStore.commit();
  },

  onRestoreDefaults: function engineManager_onRestoreDefaults() {
    var num = gEngineView._engineStore.restoreDefaultEngines();
    gEngineView.rowCountChanged(0, num);
    gEngineView.invalidate();
  },

  showRestoreDefaults: function engineManager_showRestoreDefaults(val) {
    document.documentElement.getButton("extra2").disabled = !val;
  },

  loadAddEngines: function engineManager_loadAddEngines() {
    this.onOK();
    window.opener.BrowserSearch.loadAddEngines();
    window.close();
  },

  remove: function engineManager_remove() {
    gEngineView._engineStore.removeEngine(gEngineView.selectedEngine);
    var index = gEngineView.selectedIndex;
    gEngineView.rowCountChanged(index, -1);
    gEngineView.invalidate();
    gEngineView.selection.select(Math.min(index, gEngineView.lastIndex));
    gEngineView.ensureRowIsVisible(gEngineView.currentIndex);
    document.getElementById("engineList").focus();
  },

  /**
   * Moves the selected engine either up or down in the engine list
   * @param aDir
   *        -1 to move the selected engine down, +1 to move it up.
   */
  bump: function engineManager_move(aDir) {
    var selectedEngine = gEngineView.selectedEngine;
    var newIndex = gEngineView.selectedIndex - aDir;

    gEngineView._engineStore.moveEngine(selectedEngine, newIndex);

    gEngineView.invalidate();
    gEngineView.selection.select(newIndex);
    gEngineView.ensureRowIsVisible(newIndex);
    this.showRestoreDefaults(true);
    document.getElementById("engineList").focus();
  },

  editKeyword: function engineManager_editKeyword() {
    var selectedEngine = gEngineView.selectedEngine;
    if (!selectedEngine)
      return;

    var alias = { value: selectedEngine.alias };
    var strings = document.getElementById("engineManagerBundle");
    var title = strings.getString("editTitle");
    var msg = strings.getFormattedString("editMsg", [selectedEngine.name]);

    while (Services.prompt.prompt(window, title, msg, alias, null, {})) {
      var bduplicate = false;
      var eduplicate = false;
      var dupName = "";

      if (alias.value != "") {
        try {
          let bmserv = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
                       getService(Ci.nsINavBookmarksService);
          if (bmserv.getURIForKeyword(alias.value))
            bduplicate = true;
        } catch(ex) {}

        // Check for duplicates in changes we haven't committed yet
        let engines = gEngineView._engineStore.engines;
        for each (let engine in engines) {
          if (engine.alias == alias.value &&
              engine.name != selectedEngine.name) {
            eduplicate = true;
            dupName = engine.name;
            break;
          }
        }
      }

      // Notify the user if they have chosen an existing engine/bookmark keyword
      if (eduplicate || bduplicate) {
        var dtitle = strings.getString("duplicateTitle");
        var bmsg = strings.getString("duplicateBookmarkMsg");
        var emsg = strings.getFormattedString("duplicateEngineMsg", [dupName]);

        Services.prompt.alert(window, dtitle, eduplicate ? emsg : bmsg);
      } else {
        gEngineView._engineStore.changeEngine(selectedEngine, "alias",
                                              alias.value);
        gEngineView.invalidate();
        break;
      }
    }
  },

  onSelect: function engineManager_onSelect() {
    // Buttons only work if an engine is selected and it's not the last engine,
    // the latter is true when the selected is first and last at the same time.
    var lastSelected = (gEngineView.selectedIndex == gEngineView.lastIndex);
    var firstSelected = (gEngineView.selectedIndex == 0);
    var noSelection = (gEngineView.selectedIndex == -1);

    document.getElementById("cmd_remove")
            .setAttribute("disabled", noSelection ||
                                      (firstSelected && lastSelected));

    document.getElementById("cmd_moveup")
            .setAttribute("disabled", noSelection || firstSelected);

    document.getElementById("cmd_movedown")
            .setAttribute("disabled", noSelection || lastSelected);

    document.getElementById("cmd_editkeyword")
            .setAttribute("disabled", noSelection);
  }
};

function onDragEngineStart(event) {
  var selectedIndex = gEngineView.selectedIndex;
  if (selectedIndex >= 0) {
    event.dataTransfer.setData(ENGINE_FLAVOR, selectedIndex.toString());
    event.dataTransfer.effectAllowed = "move";
  }
}

// "Operation" objects
function EngineMoveOp(aEngineClone, aNewIndex) {
  if (!aEngineClone)
    throw new Error("bad args to new EngineMoveOp!");
  this._engine = aEngineClone.originalEngine;
  this._newIndex = aNewIndex;
}
EngineMoveOp.prototype = {
  _engine: null,
  _newIndex: null,
  commit: function EMO_commit() {
    Services.search.moveEngine(this._engine, this._newIndex);
  }
}

function EngineRemoveOp(aEngineClone) {
  if (!aEngineClone)
    throw new Error("bad args to new EngineRemoveOp!");
  this._engine = aEngineClone.originalEngine;
}
EngineRemoveOp.prototype = {
  _engine: null,
  commit: function ERO_commit() {
    Services.search.removeEngine(this._engine);
  }
}

function EngineUnhideOp(aEngineClone, aNewIndex) {
  if (!aEngineClone)
    throw new Error("bad args to new EngineUnhideOp!");
  this._engine = aEngineClone.originalEngine;
  this._newIndex = aNewIndex;
}
EngineUnhideOp.prototype = {
  _engine: null,
  _newIndex: null,
  commit: function EUO_commit() {
    this._engine.hidden = false;
    Services.search.moveEngine(this._engine, this._newIndex);
  }
}

function EngineChangeOp(aEngineClone, aProp, aValue) {
  if (!aEngineClone)
    throw new Error("bad args to new EngineChangeOp!");

  this._engine = aEngineClone.originalEngine;
  this._prop = aProp;
  this._newValue = aValue;
}
EngineChangeOp.prototype = {
  _engine: null,
  _prop: null,
  _newValue: null,
  commit: function ECO_commit() {
    this._engine[this._prop] = this._newValue;
  }
}

function EngineStore() {
  this._engines = Services.search.getVisibleEngines().map(this._cloneEngine);
  this._defaultEngines = Services.search.getDefaultEngines().map(this._cloneEngine);

  this._ops = [];

  // check if we need to disable the restore defaults button
  var someHidden = this._defaultEngines.some(function (e) e.hidden);
  gEngineManagerDialog.showRestoreDefaults(someHidden);
}
EngineStore.prototype = {
  _engines: null,
  _defaultEngines: null,
  _ops: null,

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
    for each (var engine in this._engines)
      if (engine.name == aName)
        return engine;

    return null;
  },

  _cloneEngine: function ES_cloneEngine(aEngine) {
    var clonedObj={};
    for (var i in aEngine)
      clonedObj[i] = aEngine[i];
    clonedObj.originalEngine = aEngine;
    return clonedObj;
  },

  // Callback for Array's some(). A thisObj must be passed to some()
  _isSameEngine: function ES_isSameEngine(aEngineClone) {
    return aEngineClone.originalEngine == this.originalEngine;
  },

  commit: function ES_commit() {
    var currentEngine = this._cloneEngine(Services.search.currentEngine);
    for (var i = 0; i < this._ops.length; i++)
      this._ops[i].commit();

    // Restore currentEngine if it is a default engine that is still visible.
    // Needed if the user deletes currentEngine and then restores it.
    if (this._defaultEngines.some(this._isSameEngine, currentEngine) &&
        !currentEngine.originalEngine.hidden)
      Services.search.currentEngine = currentEngine.originalEngine;
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

    this._ops.push(new EngineMoveOp(aEngine, aNewIndex));
  },

  removeEngine: function ES_removeEngine(aEngine) {
    var index = this._getIndexForEngine(aEngine);
    if (index == -1)
      throw new Error("invalid engine?");

    this._engines.splice(index, 1);
    this._ops.push(new EngineRemoveOp(aEngine));
    if (this._defaultEngines.some(this._isSameEngine, aEngine))
      gEngineManagerDialog.showRestoreDefaults(true);
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
        this._engines.splice(i, 0, e);
        this._ops.push(new EngineUnhideOp(e, i));
        added++;
      }
    }
    gEngineManagerDialog.showRestoreDefaults(false);
    return added;
  },

  changeEngine: function ES_changeEngine(aEngine, aProp, aNewValue) {
    var index = this._getIndexForEngine(aEngine);
    if (index == -1)
      throw new Error("invalid engine?");

    this._engines[index][aProp] = aNewValue;
    this._ops.push(new EngineChangeOp(aEngine, aProp, aNewValue));
  },

  reloadIcons: function ES_reloadIcons() {
    this._engines.forEach(function (e) {
      e.uri = e.originalEngine.uri;
    });
  }
}

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

  // nsITreeView
  get rowCount() {
    return this._engineStore.engines.length;
  },

  getImageSrc: function(index, column) {
    if (column.id == "engineName" && this._engineStore.engines[index].iconURI)
      return this._engineStore.engines[index].iconURI.spec;
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

    if (dropIndex > sourceIndex) {
      if (orientation == Ci.nsITreeView.DROP_BEFORE)
        dropIndex--;
    } else {
      if (orientation == Ci.nsITreeView.DROP_AFTER)
        dropIndex++;
    }

    this._engineStore.moveEngine(sourceEngine, dropIndex);
    gEngineManagerDialog.showRestoreDefaults(true);

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
  getCellValue: function(index, column) { },
  toggleOpenState: function(index) { },
  cycleHeader: function(column) { },
  selectionChanged: function() { },
  cycleCell: function(row, column) { },
  isEditable: function(index, column) { return false; },
  isSelectable: function(index, column) { return false; },
  setCellValue: function(index, column, value) { },
  setCellText: function(index, column, value) { },
  performAction: function(action) { },
  performActionOnRow: function(action, index) { },
  performActionOnCell: function(action, index, column) { }
};
