# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is the Browser Search Service.
#
# The Initial Developer of the Original Code is
# Google Inc.
# Portions created by the Initial Developer are Copyright (C) 2005
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Ben Goodger <beng@google.com> (Original author)
#   Gavin Sharp <gavin@gavinsharp.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

const Ci = Components.interfaces;
const Cc = Components.classes;

var gEngineView = null;

var gEngineManagerDialog = {
  init: function engineManager_init() {
    gEngineView = new EngineView(new EngineStore());

    var tree = document.getElementById("engineList");
    tree.view = gEngineView;

    var os = Cc["@mozilla.org/observer-service;1"].
             getService(Ci.nsIObserverService);
    os.addObserver(this, "browser-search-engine-modified", false);
  },

  observe: function engineManager_observe(aEngine, aTopic, aVerb) {
    if (aTopic == "browser-search-engine-modified") {
      aEngine.QueryInterface(Ci.nsISearchEngine)
      switch (aVerb) {
      case "engine-added":
        gEngineView._engineStore.addEngine(aEngine);
        gEngineView.rowCountChanged(gEngineView.lastIndex, 1);
        break;
      case "engine-changed":
        gEngineView._enginesStore.reloadIcons();
        break;
      case "engine-removed":
      case "engine-current":
        // Not relevant
        return;
      }
      gEngineView.invalidate();
    }
  },

  onOK: function engineManager_onOK() {
    // Remove the observer
    var os = Cc["@mozilla.org/observer-service;1"].
             getService(Ci.nsIObserverService);
    os.removeObserver(this, "browser-search-engine-modified");

    // Commit the changes
    gEngineView._engineStore.commit();
  },
  
  onCancel: function engineManager_onCancel() {
    // Remove the observer
    var os = Cc["@mozilla.org/observer-service;1"].
             getService(Ci.nsIObserverService);
    os.removeObserver(this, "browser-search-engine-modified");
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
  },

  /**
   * Moves the selected engine to the specified index in the engine list.
   * @param aNewIndex
   *        The desired index of the engine in the list.
   */
  moveToIndex: function engineManager_moveToIndex(aNewIndex) {
    var selectedEngine = gEngineView.selectedEngine;
    gEngineView._engineStore.moveEngine(selectedEngine, aNewIndex);

    gEngineView.invalidate();
    gEngineView.selection.select(aNewIndex);
  },

  onSelect: function engineManager_onSelect() {
    var noEngineSelected = (gEngineView.selectedIndex == -1);
    var lastSelected = (gEngineView.selectedIndex == gEngineView.lastIndex);
    var firstSelected = (gEngineView.selectedIndex == 0);

    document.getElementById("cmd_remove").setAttribute("disabled",
                                                       noEngineSelected);

    document.getElementById("cmd_moveup").setAttribute("disabled",
                                            noEngineSelected || firstSelected);

    document.getElementById("cmd_movedown").setAttribute("disabled",
                                             noEngineSelected || lastSelected);
  }
};

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
    var searchService = Cc["@mozilla.org/browser/search-service;1"].
                        getService(Ci.nsIBrowserSearchService);
    searchService.moveEngine(this._engine, this._newIndex);
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
    var searchService = Cc["@mozilla.org/browser/search-service;1"].
                        getService(Ci.nsIBrowserSearchService);
    searchService.removeEngine(this._engine);
  }
}

function EngineStore() {
  var searchService = Cc["@mozilla.org/browser/search-service;1"].
                      getService(Ci.nsIBrowserSearchService);
  this._engines = searchService.getVisibleEngines({}).map(this._cloneEngine);

  this._ops = [];
}
EngineStore.prototype = {
  _engines: null,
  _ops: null,

  get engines() {
    return this._engines;
  },
  set engines(val) {
    this._engines = val;
  },

  _getIndexForEngine: function ES_getIndexForEngine(aEngine) {
    return this._engines.indexOf(aEngine);
  },

  _cloneEngine: function ES_cloneObj(aEngine) {
    var newO=[];
    for (var i in aEngine)
      newO[i] = aEngine[i];
    newO.originalEngine = aEngine;
    return newO;
  },

  commit: function ES_commit() {
    for (var i = 0; i < this._ops.length; i++)
      this._ops[i].commit();
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

    // Switch the two engines in our internal store
    this._engines[index] = this._engines[aNewIndex];
    this._engines[aNewIndex] = aEngine;

    this._ops.push(new EngineMoveOp(aEngine, aNewIndex));
  },

  removeEngine: function ES_removeEngine(aEngine) {
    var index = this._getIndexForEngine(aEngine);
    if (index == -1)
      throw new Error("invalid engine?");
 
    this._engines.splice(index, 1);
    this._ops.push(new EngineRemoveOp(aEngine));
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
      var min = { };
      seln.getRangeAt(0, min, { });
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
    return "";
  },

  setTree: function(tree) {
    this.tree = tree;
  },

  selection: null,
  getRowProperties: function(index, properties) { },
  getCellProperties: function(index, column, properties) { },
  getColumnProperties: function(column, properties) { },
  isContainer: function(index) { return false; },
  isContainerOpen: function(index) { return false; },
  isContainerEmpty: function(index) { return false; },
  isSeparator: function(index) { return false; },
  isSorted: function(index) { return false; },
  canDrop: function(index) { return false; },
  drop: function(index, orientation) { },
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
