/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Firefox Sync.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Philipp von Weitershausen <philipp@weitershausen.de>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://services-sync/main.js");
Cu.import("resource://gre/modules/DownloadUtils.jsm");

let gSyncQuota = {

  init: function init() {
    this.bundle = document.getElementById("quotaStrings");
    let caption = document.getElementById("treeCaption");
    caption.firstChild.nodeValue = this.bundle.getString("quota.treeCaption.label");

    gUsageTreeView.init();
    this.tree = document.getElementById("usageTree");
    this.tree.view = gUsageTreeView;

    this.loadData();
  },

  loadData: function loadData() {
    this._usage_req = Weave.Service.getStorageInfo(Weave.INFO_COLLECTION_USAGE,
                                                   function (error, usage) {
      delete gSyncQuota._usage_req;
      // displayUsageData handles null values, so no need to check 'error'.
      gUsageTreeView.displayUsageData(usage);
    });

    let usageLabel = document.getElementById("usageLabel");
    let bundle = this.bundle;

    this._quota_req = Weave.Service.getStorageInfo(Weave.INFO_QUOTA,
                                                   function (error, quota) {
      delete gSyncQuota._quota_req;

      if (error) {
        usageLabel.value = bundle.getString("quota.usageError.label");
        return;
      }
      let used = gSyncQuota.convertKB(quota[0]);
      if (!quota[1]) {
        // No quota on the server.
        usageLabel.value = bundle.getFormattedString(
          "quota.usageNoQuota.label", used);
        return;
      }
      let percent = Math.round(100 * quota[0] / quota[1]);
      let total = gSyncQuota.convertKB(quota[1]);
      usageLabel.value = bundle.getFormattedString(
        "quota.usagePercentage.label", [percent].concat(used).concat(total));
    });
  },

  onCancel: function onCancel() {
    if (this._usage_req) {
      this._usage_req.abort();
    }
    if (this._quota_req) {
      this._quota_req.abort();
    }
    return true;
  },

  onAccept: function onAccept() {
    let engines = gUsageTreeView.getEnginesToDisable();
    for each (let engine in engines) {
      Weave.Engines.get(engine).enabled = false;
    }
    if (engines.length) {
      // The 'Weave' object will disappear once the window closes.
      let Service = Weave.Service;
      Weave.Utils.nextTick(function() { Service.sync(); });
    }
    return this.onCancel();
  },

  convertKB: function convertKB(value) {
    return DownloadUtils.convertByteUnits(value * 1024);
  }

};

let gUsageTreeView = {

  _ignored: {keys: true,
             meta: true,
             clients: true},

  /*
   * Internal data structures underlaying the tree.
   */
  _collections: [],
  _byname: {},

  init: function init() {
    let retrievingLabel = gSyncQuota.bundle.getString("quota.retrieving.label");
    for each (let engine in Weave.Engines.getEnabled()) {
      if (this._ignored[engine.name])
        continue;

      // Some engines use the same pref, which means they can only be turned on
      // and off together. We need to combine them here as well.
      let existing = this._byname[engine.prefName];
      if (existing) {
        existing.engines.push(engine.name);
        continue;
      }

      let obj = {name: engine.prefName,
                 title: this._collectionTitle(engine),
                 engines: [engine.name],
                 enabled: true,
                 sizeLabel: retrievingLabel};
      this._collections.push(obj);
      this._byname[engine.prefName] = obj;
    }
  },

  _collectionTitle: function _collectionTitle(engine) {
    try {
      return gSyncQuota.bundle.getString(
        "collection." + engine.prefName + ".label");
    } catch (ex) {
      return engine.Name;
    }
  },

  /*
   * Process the quota information as returned by info/collection_usage.
   */
  displayUsageData: function displayUsageData(data) {
    for each (let coll in this._collections) {
      coll.size = 0;
      // If we couldn't retrieve any data, just blank out the label.
      if (!data) {
        coll.sizeLabel = "";
        continue;
      }

      for each (let engineName in coll.engines)
        coll.size += data[engineName] || 0;
      let sizeLabel = "";
      sizeLabel = gSyncQuota.bundle.getFormattedString(
        "quota.sizeValueUnit.label", gSyncQuota.convertKB(coll.size));
      coll.sizeLabel = sizeLabel;
    }
    let sizeColumn = this.treeBox.columns.getNamedColumn("size");
    this.treeBox.invalidateColumn(sizeColumn);
  },

  /*
   * Handle click events on the tree.
   */
  onTreeClick: function onTreeClick(event) {
    if (event.button == 2)
      return;

    let row = {}, col = {};
    this.treeBox.getCellAt(event.clientX, event.clientY, row, col, {});
    if (col.value && col.value.id == "enabled")
      this.toggle(row.value);
  },

  /*
   * Toggle enabled state of an engine.
   */
  toggle: function toggle(row) {
    // Update the tree
    let collection = this._collections[row];
    collection.enabled = !collection.enabled;
    this.treeBox.invalidateRow(row);

    // Display which ones will be removed 
    let freeup = 0;
    let toremove = [];
    for each (collection in this._collections) {
      if (collection.enabled)
        continue;
      toremove.push(collection.name);
      freeup += collection.size;
    }

    let caption = document.getElementById("treeCaption");
    if (!toremove.length) {
      caption.className = "";
      caption.firstChild.nodeValue = gSyncQuota.bundle.getString(
        "quota.treeCaption.label");
      return;
    }

    toremove = [this._byname[coll].title for each (coll in toremove)];
    toremove = toremove.join(gSyncQuota.bundle.getString("quota.list.separator"));
    caption.firstChild.nodeValue = gSyncQuota.bundle.getFormattedString(
      "quota.removal.label", [toremove]);
    if (freeup)
      caption.firstChild.nodeValue += gSyncQuota.bundle.getFormattedString(
        "quota.freeup.label", gSyncQuota.convertKB(freeup));
    caption.className = "captionWarning";
  },

  /*
   * Return a list of engines (or rather their pref names) that should be
   * disabled.
   */
  getEnginesToDisable: function getEnginesToDisable() {
    return [coll.name for each (coll in this._collections) if (!coll.enabled)];
  },

  // nsITreeView

  get rowCount() {
    return this._collections.length;
  },

  getRowProperties: function(index, properties) {},
  getCellProperties: function(row, col, properties) {},
  getColumnProperties: function(col, properties) {},
  isContainer: function(index) { return false; },
  isContainerOpen: function(index) { return false; },
  isContainerEmpty: function(index) { return false; },
  isSeparator: function(index) { return false; },
  isSorted: function() { return false; },
  canDrop: function(index, orientation, dataTransfer) { return false; },
  drop: function(row, orientation, dataTransfer) {},
  getParentIndex: function(rowIndex) {},
  hasNextSibling: function(rowIndex, afterIndex) { return false; },
  getLevel: function(index) { return 0; },
  getImageSrc: function(row, col) {},

  getCellValue: function(row, col) {
    return this._collections[row].enabled;
  },

  getCellText: function getCellText(row, col) {
    let collection = this._collections[row];
    switch (col.id) {
      case "collection":
        return collection.title;
      case "size":
        return collection.sizeLabel;
      default:
        return "";
    }
  },

  setTree: function setTree(tree) {
    this.treeBox = tree;
  },

  toggleOpenState: function(index) {},
  cycleHeader: function(col) {},
  selectionChanged: function() {},
  cycleCell: function(row, col) {},
  isEditable: function(row, col) { return false; },
  isSelectable: function (row, col) { return false; },
  setCellValue: function(row, col, value) {},
  setCellText: function(row, col, value) {},
  performAction: function(action) {},
  performActionOnRow: function(action, row) {},
  performActionOnCell: function(action, row, col) {}

};
