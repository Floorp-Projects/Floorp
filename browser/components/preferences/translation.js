/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyGetter(this, "gLangBundle", () =>
  Services.strings.createBundle("chrome://global/locale/languageNames.properties"));

const kPermissionType = "translate";
const kLanguagesPref = "browser.translation.neverForLanguages";

function Tree(aId, aData) {
  this._data = aData;
  this._tree = document.getElementById(aId);
  this._tree.view = this;
}

Tree.prototype = {
  get boxObject() {
    return this._tree.treeBoxObject;
  },
  get isEmpty() {
    return !this._data.length;
  },
  get hasSelection() {
    return this.selection.count > 0;
  },
  getSelectedItems() {
    let result = [];

    let rc = this.selection.getRangeCount();
    for (let i = 0; i < rc; ++i) {
      let min = {}, max = {};
      this.selection.getRangeAt(i, min, max);
      for (let j = min.value; j <= max.value; ++j)
        result.push(this._data[j]);
    }

    return result;
  },

  // nsITreeView implementation
  get rowCount() {
    return this._data.length;
  },
  getCellText(aRow, aColumn) {
    return this._data[aRow];
  },
  isSeparator(aIndex) {
    return false;
  },
  isSorted() {
    return false;
  },
  isContainer(aIndex) {
    return false;
  },
  setTree(aTree) {},
  getImageSrc(aRow, aColumn) {},
  getProgressMode(aRow, aColumn) {},
  getCellValue(aRow, aColumn) {},
  cycleHeader(column) {},
  getRowProperties(row) {
    return "";
  },
  getColumnProperties(column) {
    return "";
  },
  getCellProperties(row, column) {
    return "";
  },
  QueryInterface: XPCOMUtils.generateQI([Ci.nsITreeView])
};

function Lang(aCode) {
  this.langCode = aCode;
  this._label = gLangBundle.GetStringFromName(aCode);
}

Lang.prototype = {
  toString() {
    return this._label;
  }
}

var gTranslationExceptions = {
  onLoad() {
    if (this._siteTree) {
      // Re-using an open dialog, clear the old observers.
      this.uninit();
    }

    // Load site permissions into an array.
    this._sites = [];
    let enumerator = Services.perms.enumerator;
    while (enumerator.hasMoreElements()) {
      let perm = enumerator.getNext().QueryInterface(Ci.nsIPermission);

      if (perm.type == kPermissionType &&
          perm.capability == Services.perms.DENY_ACTION) {
        this._sites.push(perm.principal.origin);
      }
    }
    Services.obs.addObserver(this, "perm-changed", false);
    this._sites.sort();

    this._siteTree = new Tree("sitesTree", this._sites);
    this.onSiteSelected();

    this._langs = this.getLanguageExceptions();
    Services.prefs.addObserver(kLanguagesPref, this, false);
    this._langTree = new Tree("languagesTree", this._langs);
    this.onLanguageSelected();
  },

  // Get the list of languages we don't translate as an array.
  getLanguageExceptions() {
    let langs = Services.prefs.getCharPref(kLanguagesPref);
    if (!langs)
      return [];

    let result = langs.split(",").map(code => new Lang(code));
    result.sort();

    return result;
  },

  observe(aSubject, aTopic, aData) {
    if (aTopic == "perm-changed") {
      if (aData == "cleared") {
        if (!this._sites.length)
          return;
        let removed = this._sites.splice(0, this._sites.length);
        this._siteTree.boxObject.rowCountChanged(0, -removed.length);
      } else {
        let perm = aSubject.QueryInterface(Ci.nsIPermission);
        if (perm.type != kPermissionType)
          return;

        if (aData == "added") {
          if (perm.capability != Services.perms.DENY_ACTION)
            return;
          this._sites.push(perm.principal.origin);
          this._sites.sort();
          let boxObject = this._siteTree.boxObject;
          boxObject.rowCountChanged(0, 1);
          boxObject.invalidate();
        } else if (aData == "deleted") {
          let index = this._sites.indexOf(perm.principal.origin);
          if (index == -1)
            return;
          this._sites.splice(index, 1);
          this._siteTree.boxObject.rowCountChanged(index, -1);
          this.onSiteSelected();
          return;
        }
      }
      this.onSiteSelected();
    } else if (aTopic == "nsPref:changed") {
      this._langs = this.getLanguageExceptions();
      let change = this._langs.length - this._langTree.rowCount;
      this._langTree._data = this._langs;
      let boxObject = this._langTree.boxObject;
      if (change)
        boxObject.rowCountChanged(0, change);
      boxObject.invalidate();
      this.onLanguageSelected();
    }
  },

  _handleButtonDisabling(aTree, aIdPart) {
    let empty = aTree.isEmpty;
    document.getElementById("removeAll" + aIdPart + "s").disabled = empty;
    document.getElementById("remove" + aIdPart).disabled =
      empty || !aTree.hasSelection;
  },

  onLanguageSelected() {
    this._handleButtonDisabling(this._langTree, "Language");
  },

  onSiteSelected() {
    this._handleButtonDisabling(this._siteTree, "Site");
  },

  onLanguageDeleted() {
    let langs = Services.prefs.getCharPref(kLanguagesPref);
    if (!langs)
      return;

    let removed = this._langTree.getSelectedItems().map(l => l.langCode);

    langs = langs.split(",").filter(l => removed.indexOf(l) == -1);
    Services.prefs.setCharPref(kLanguagesPref, langs.join(","));
  },

  onAllLanguagesDeleted() {
    Services.prefs.setCharPref(kLanguagesPref, "");
  },

  onSiteDeleted() {
    let removedSites = this._siteTree.getSelectedItems();
    for (let origin of removedSites) {
      let principal = Services.scriptSecurityManager.createCodebasePrincipalFromOrigin(origin);
      Services.perms.removeFromPrincipal(principal, kPermissionType);
    }
  },

  onAllSitesDeleted() {
    if (this._siteTree.isEmpty)
      return;

    let removedSites = this._sites.splice(0, this._sites.length);
    this._siteTree.boxObject.rowCountChanged(0, -removedSites.length);

    for (let origin of removedSites) {
      let principal = Services.scriptSecurityManager.createCodebasePrincipalFromOrigin(origin);
      Services.perms.removeFromPrincipal(principal, kPermissionType);
    }

    this.onSiteSelected();
  },

  onSiteKeyPress(aEvent) {
    if (aEvent.keyCode == KeyEvent.DOM_VK_DELETE)
      this.onSiteDeleted();
  },

  onLanguageKeyPress(aEvent) {
    if (aEvent.keyCode == KeyEvent.DOM_VK_DELETE)
      this.onLanguageDeleted();
  },

  onWindowKeyPress(aEvent) {
    if (aEvent.keyCode == KeyEvent.DOM_VK_ESCAPE)
      window.close();
  },

  uninit() {
    Services.obs.removeObserver(this, "perm-changed");
    Services.prefs.removeObserver(kLanguagesPref, this);
  }
};
