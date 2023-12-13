/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * The permission type to give to Services.perms for Translations.
 */
const TRANSLATIONS_PERMISSION = "translations";
/**
 * The list of BCP-47 language tags that will trigger auto-translate.
 */
const ALWAYS_TRANSLATE_LANGS_PREF =
  "browser.translations.alwaysTranslateLanguages";
/**
 * The list of BCP-47 language tags that will prevent showing Translations UI.
 */
const NEVER_TRANSLATE_LANGS_PREF =
  "browser.translations.neverTranslateLanguages";

ChromeUtils.defineESModuleGetters(this, {
  TranslationsParent: "resource://gre/actors/TranslationsParent.sys.mjs",
});

function Tree(aId, aData) {
  this._data = aData;
  this._tree = document.getElementById(aId);
  this._tree.view = this;
}

Tree.prototype = {
  get tree() {
    return this._tree;
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
      let min = {},
        max = {};
      this.selection.getRangeAt(i, min, max);
      for (let j = min.value; j <= max.value; ++j) {
        result.push(this._data[j]);
      }
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
  QueryInterface: ChromeUtils.generateQI(["nsITreeView"]),
};

function Lang(aCode, label) {
  this.langCode = aCode;
  this._label = label;
}

Lang.prototype = {
  toString() {
    return this._label;
  },
};

var gTranslationsSettings = {
  onLoad() {
    if (this._neverTranslateSiteTree) {
      // Re-using an open dialog, clear the old observers.
      this.removeObservers();
    }

    // Load site permissions into an array.
    this._neverTranslateSites = TranslationsParent.listNeverTranslateSites();

    // Load language tags into arrays.
    this._alwaysTranslateLangs = this.getAlwaysTranslateLanguages();
    this._neverTranslateLangs = this.getNeverTranslateLanguages();

    // Add observers for relevant prefs and permissions.
    Services.obs.addObserver(this, "perm-changed");
    Services.prefs.addObserver(ALWAYS_TRANSLATE_LANGS_PREF, this);
    Services.prefs.addObserver(NEVER_TRANSLATE_LANGS_PREF, this);

    // Build trees from the arrays.
    this._alwaysTranslateLangsTree = new Tree(
      "alwaysTranslateLanguagesTree",
      this._alwaysTranslateLangs
    );
    this._neverTranslateLangsTree = new Tree(
      "neverTranslateLanguagesTree",
      this._neverTranslateLangs
    );
    this._neverTranslateSiteTree = new Tree(
      "neverTranslateSitesTree",
      this._neverTranslateSites
    );

    // Ensure the UI for each group is in the correct state.
    this.onSelectAlwaysTranslateLanguage();
    this.onSelectNeverTranslateLanguage();
    this.onSelectNeverTranslateSite();
  },

  /**
   * Retrieves the value of a char-pref splits its value into an
   * array delimited by commas.
   *
   * This is used for the translations preferences which are comma-
   * separated lists of BCP-47 language tags.
   *
   * @param {string} pref
   * @returns {Array<string>}
   */
  getLangsFromPref(pref) {
    let rawLangs = Services.prefs.getCharPref(pref);
    if (!rawLangs) {
      return [];
    }

    let langArr = rawLangs.split(",");
    let displayNames = Services.intl.getLanguageDisplayNames(
      undefined,
      langArr
    );
    let langs = langArr.map((lang, i) => new Lang(lang, displayNames[i]));
    langs.sort();

    return langs;
  },

  /**
   * Retrieves the always-translate language tags as an array.
   * @returns {Array<string>}
   */
  getAlwaysTranslateLanguages() {
    return this.getLangsFromPref(ALWAYS_TRANSLATE_LANGS_PREF);
  },

  /**
   * Retrieves the never-translate language tags as an array.
   * @returns {Array<string>}
   */
  getNeverTranslateLanguages() {
    return this.getLangsFromPref(NEVER_TRANSLATE_LANGS_PREF);
  },

  /**
   * Handles updating the UI components on pref or permission changes.
   */
  observe(aSubject, aTopic, aData) {
    if (aTopic === "perm-changed") {
      if (aData === "cleared") {
        // Permissions have been cleared
        if (!this._neverTranslateSites.length) {
          // There were no sites with permissions set, nothing to do.
          return;
        }
        // Update the tree based on the amount of permissions removed.
        let removed = this._neverTranslateSites.splice(
          0,
          this._neverTranslateSites.length
        );
        this._neverTranslateSiteTree.tree.rowCountChanged(0, -removed.length);
      } else {
        let perm = aSubject.QueryInterface(Ci.nsIPermission);
        if (perm.type != TRANSLATIONS_PERMISSION) {
          // The updated permission was not for Translations, nothing to do.
          return;
        }
        if (aData === "added") {
          if (perm.capability != Services.perms.DENY_ACTION) {
            // We are only showing data for sites we should never translate.
            // If the permission is not DENY_ACTION, we don't care about it here.
            return;
          }
          this._neverTranslateSites.push(perm.principal.origin);
          this._neverTranslateSites.sort();
          let tree = this._neverTranslateSiteTree.tree;
          tree.rowCountChanged(0, 1);
          tree.invalidate();
        } else if (aData == "deleted") {
          let index = this._neverTranslateSites.indexOf(perm.principal.origin);
          if (index == -1) {
            // The deleted permission was not in the tree, nothing to do.
            return;
          }
          this._neverTranslateSites.splice(index, 1);
          this._neverTranslateSiteTree.tree.rowCountChanged(index, -1);
        }
      }
      // Ensure the UI updates to the changes.
      this.onSelectNeverTranslateSite();
    } else if (aTopic === "nsPref:changed") {
      switch (aData) {
        case ALWAYS_TRANSLATE_LANGS_PREF: {
          this._alwaysTranslateLangs = this.getAlwaysTranslateLanguages();

          let alwaysTranslateLangsChange =
            this._alwaysTranslateLangs.length -
            this._alwaysTranslateLangsTree.rowCount;

          this._alwaysTranslateLangsTree._data = this._alwaysTranslateLangs;
          let alwaysTranslateLangsTree = this._alwaysTranslateLangsTree.tree;

          if (alwaysTranslateLangsChange) {
            alwaysTranslateLangsTree.rowCountChanged(
              0,
              alwaysTranslateLangsChange
            );
          }

          alwaysTranslateLangsTree.invalidate();

          // Ensure the UI updates to the changes.
          this.onSelectAlwaysTranslateLanguage();
          break;
        }
        case NEVER_TRANSLATE_LANGS_PREF: {
          this._neverTranslateLangs = this.getNeverTranslateLanguages();

          let neverTranslateLangsChange =
            this._neverTranslateLangs.length -
            this._neverTranslateLangsTree.rowCount;

          this._neverTranslateLangsTree._data = this._neverTranslateLangs;
          let neverTranslateLangsTree = this._neverTranslateLangsTree.tree;

          if (neverTranslateLangsChange) {
            neverTranslateLangsTree.rowCountChanged(
              0,
              neverTranslateLangsChange
            );
          }

          neverTranslateLangsTree.invalidate();

          // Ensure the UI updates to the changes.
          this.onSelectNeverTranslateLanguage();
          break;
        }
      }
    }
  },

  /**
   * Ensures that buttons states are enabled/disabled accordingly based on the
   * content of the trees.
   *
   * The remove button should be enabled only if an item is selected.
   * The removeAll button should be enabled any time the tree has content.
   *
   * @param {Tree} aTree
   * @param {string} aIdPart
   */
  _handleButtonDisabling(aTree, aIdPart) {
    let empty = aTree.isEmpty;
    document.getElementById("removeAll" + aIdPart + "s").disabled = empty;
    document.getElementById("remove" + aIdPart).disabled =
      empty || !aTree.hasSelection;
  },

  /**
   * Updates the UI state for the always-translate languages section.
   */
  onSelectAlwaysTranslateLanguage() {
    this._handleButtonDisabling(
      this._alwaysTranslateLangsTree,
      "AlwaysTranslateLanguage"
    );
  },

  /**
   * Updates the UI state for the never-translate languages section.
   */
  onSelectNeverTranslateLanguage() {
    this._handleButtonDisabling(
      this._neverTranslateLangsTree,
      "NeverTranslateLanguage"
    );
  },

  /**
   * Updates the UI state for the never-translate sites section.
   */
  onSelectNeverTranslateSite() {
    this._handleButtonDisabling(
      this._neverTranslateSiteTree,
      "NeverTranslateSite"
    );
  },

  /**
   * Updates the value of a language pref to match when a language is removed
   * through the UI.
   *
   * @param {string} pref
   * @param {Tree} tree
   */
  _onRemoveLanguage(pref, tree) {
    let langs = Services.prefs.getCharPref(pref);
    if (!langs) {
      return;
    }

    let removed = tree.getSelectedItems().map(l => l.langCode);

    langs = langs.split(",").filter(l => !removed.includes(l));
    Services.prefs.setCharPref(pref, langs.join(","));
  },

  /**
   * Updates the never-translate language pref when a never-translate language
   * is removed via the UI.
   */
  onRemoveAlwaysTranslateLanguage() {
    this._onRemoveLanguage(
      ALWAYS_TRANSLATE_LANGS_PREF,
      this._alwaysTranslateLangsTree
    );
  },

  /**
   * Updates the always-translate language pref when a always-translate language
   * is removed via the UI.
   */
  onRemoveNeverTranslateLanguage() {
    this._onRemoveLanguage(
      NEVER_TRANSLATE_LANGS_PREF,
      this._neverTranslateLangsTree
    );
  },

  /**
   * Updates the permissions for a never-translate site when it is removed via the UI.
   */
  onRemoveNeverTranslateSite() {
    let removedNeverTranslateSites =
      this._neverTranslateSiteTree.getSelectedItems();
    for (let origin of removedNeverTranslateSites) {
      TranslationsParent.setNeverTranslateSiteByOrigin(false, origin);
    }
  },

  /**
   * Clears the always-translate languages pref when the list is cleared in the UI.
   */
  onRemoveAllAlwaysTranslateLanguages() {
    Services.prefs.setCharPref(ALWAYS_TRANSLATE_LANGS_PREF, "");
  },

  /**
   * Clears the never-translate languages pref when the list is cleared in the UI.
   */
  onRemoveAllNeverTranslateLanguages() {
    Services.prefs.setCharPref(NEVER_TRANSLATE_LANGS_PREF, "");
  },

  /**
   * Clears the never-translate sites pref when the list is cleared in the UI.
   */
  onRemoveAllNeverTranslateSites() {
    if (this._neverTranslateSiteTree.isEmpty) {
      return;
    }

    let removedNeverTranslateSites = this._neverTranslateSites.splice(
      0,
      this._neverTranslateSites.length
    );
    this._neverTranslateSiteTree.tree.rowCountChanged(
      0,
      -removedNeverTranslateSites.length
    );

    for (let origin of removedNeverTranslateSites) {
      TranslationsParent.setNeverTranslateSiteByOrigin(false, origin);
    }

    this.onSelectNeverTranslateSite();
  },

  /**
   * Handles removing a selected always-translate language via the keyboard.
   */
  onAlwaysTranslateLanguageKeyPress(aEvent) {
    if (aEvent.keyCode == KeyEvent.DOM_VK_DELETE) {
      this.onRemoveAlwaysTranslateLanguage();
    }
  },

  /**
   * Handles removing a selected never-translate language via the keyboard.
   */
  onNeverTranslateLanguageKeyPress(aEvent) {
    if (aEvent.keyCode == KeyEvent.DOM_VK_DELETE) {
      this.onRemoveNeverTranslateLanguage();
    }
  },

  /**
   * Handles removing a selected never-translate site via the keyboard.
   */
  onNeverTranslateSiteKeyPress(aEvent) {
    if (aEvent.keyCode == KeyEvent.DOM_VK_DELETE) {
      this.onRemoveNeverTranslateSite();
    }
  },

  /**
   * Removes any active preference and permissions observers.
   */
  removeObservers() {
    Services.obs.removeObserver(this, "perm-changed");
    Services.prefs.removeObserver(ALWAYS_TRANSLATE_LANGS_PREF, this);
    Services.prefs.removeObserver(NEVER_TRANSLATE_LANGS_PREF, this);
  },
};
