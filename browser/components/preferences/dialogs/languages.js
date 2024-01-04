/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from /toolkit/content/preferencesBindings.js */

document
  .getElementById("LanguagesDialog")
  .addEventListener("dialoghelp", window.top.openPrefsHelp);

Preferences.addAll([
  { id: "intl.accept_languages", type: "wstring" },
  { id: "pref.browser.language.disable_button.up", type: "bool" },
  { id: "pref.browser.language.disable_button.down", type: "bool" },
  { id: "pref.browser.language.disable_button.remove", type: "bool" },
  { id: "privacy.spoof_english", type: "int" },
]);

var gLanguagesDialog = {
  _availableLanguagesList: [],
  _acceptLanguages: {},

  _selectedItemID: null,

  onLoad() {
    let spoofEnglishElement = document.getElementById("spoofEnglish");
    Preferences.addSyncFromPrefListener(spoofEnglishElement, () =>
      gLanguagesDialog.readSpoofEnglish()
    );
    Preferences.addSyncToPrefListener(spoofEnglishElement, () =>
      gLanguagesDialog.writeSpoofEnglish()
    );

    Preferences.get("intl.accept_languages").on("change", () =>
      this._readAcceptLanguages().catch(console.error)
    );

    if (!this._availableLanguagesList.length) {
      document.mozSubdialogReady = this._loadAvailableLanguages();
    }
  },

  get _activeLanguages() {
    return document.getElementById("activeLanguages");
  },

  get _availableLanguages() {
    return document.getElementById("availableLanguages");
  },

  async _loadAvailableLanguages() {
    // This is a parser for: resource://gre/res/language.properties
    // The file is formatted like so:
    // ab[-cd].accept=true|false
    //  ab = language
    //  cd = region
    var bundleAccepted = document.getElementById("bundleAccepted");

    function LocaleInfo(aLocaleName, aLocaleCode, aIsVisible) {
      this.name = aLocaleName;
      this.code = aLocaleCode;
      this.isVisible = aIsVisible;
    }

    // 1) Read the available languages out of language.properties

    let localeCodes = [];
    let localeValues = [];
    for (let currString of bundleAccepted.strings) {
      var property = currString.key.split("."); // ab[-cd].accept
      if (property[1] == "accept") {
        localeCodes.push(property[0]);
        localeValues.push(currString.value);
      }
    }

    let localeNames = Services.intl.getLocaleDisplayNames(
      undefined,
      localeCodes
    );

    for (let i in localeCodes) {
      let isVisible =
        localeValues[i] == "true" &&
        (!(localeCodes[i] in this._acceptLanguages) ||
          !this._acceptLanguages[localeCodes[i]]);

      let li = new LocaleInfo(localeNames[i], localeCodes[i], isVisible);
      this._availableLanguagesList.push(li);
    }

    await this._buildAvailableLanguageList();
    await this._readAcceptLanguages();
  },

  async _buildAvailableLanguageList() {
    var availableLanguagesPopup = document.getElementById(
      "availableLanguagesPopup"
    );
    while (availableLanguagesPopup.hasChildNodes()) {
      availableLanguagesPopup.firstChild.remove();
    }

    let frag = document.createDocumentFragment();

    // Load the UI with the data
    for (var i = 0; i < this._availableLanguagesList.length; ++i) {
      let locale = this._availableLanguagesList[i];
      let localeCode = locale.code;
      if (
        locale.isVisible &&
        (!(localeCode in this._acceptLanguages) ||
          !this._acceptLanguages[localeCode])
      ) {
        var menuitem = document.createXULElement("menuitem");
        menuitem.id = localeCode;
        document.l10n.setAttributes(menuitem, "languages-code-format", {
          locale: locale.name,
          code: localeCode,
        });
        frag.appendChild(menuitem);
      }
    }

    await document.l10n.translateFragment(frag);

    // Sort the list of languages by name
    let comp = new Services.intl.Collator(undefined, {
      usage: "sort",
    });

    let items = Array.from(frag.children);

    items.sort((a, b) => {
      return comp.compare(a.getAttribute("label"), b.getAttribute("label"));
    });

    // Re-append items in the correct order:
    items.forEach(item => frag.appendChild(item));

    availableLanguagesPopup.appendChild(frag);

    this._availableLanguages.setAttribute(
      "label",
      this._availableLanguages.getAttribute("placeholder")
    );
  },

  async _readAcceptLanguages() {
    while (this._activeLanguages.hasChildNodes()) {
      this._activeLanguages.firstChild.remove();
    }

    var selectedIndex = 0;
    var preference = Preferences.get("intl.accept_languages");
    if (preference.value == "") {
      this._activeLanguages.selectedIndex = -1;
      this.onLanguageSelect();
      return;
    }
    var languages = preference.value.toLowerCase().split(/\s*,\s*/);
    for (var i = 0; i < languages.length; ++i) {
      var listitem = document.createXULElement("richlistitem");
      var label = document.createXULElement("label");
      listitem.appendChild(label);
      listitem.id = languages[i];
      if (languages[i] == this._selectedItemID) {
        selectedIndex = i;
      }
      this._activeLanguages.appendChild(listitem);
      var localeName = this._getLocaleName(languages[i]);
      document.l10n.setAttributes(label, "languages-active-code-format", {
        locale: localeName,
        code: languages[i],
      });

      // Hash this language as an "Active" language so we don't
      // show it in the list that can be added.
      this._acceptLanguages[languages[i]] = true;
    }

    // We're forcing an early localization here because otherwise
    // the initial sizing of the dialog will happen before it and
    // result in overflow.
    await document.l10n.translateFragment(this._activeLanguages);

    if (this._activeLanguages.childNodes.length) {
      this._activeLanguages.ensureIndexIsVisible(selectedIndex);
      this._activeLanguages.selectedIndex = selectedIndex;
    }

    // Update states of accept-language list and buttons according to
    // privacy.resistFingerprinting and privacy.spoof_english.
    this.readSpoofEnglish();
  },

  onAvailableLanguageSelect() {
    var availableLanguages = this._availableLanguages;
    var addButton = document.getElementById("addButton");
    addButton.disabled =
      availableLanguages.disabled || availableLanguages.selectedIndex < 0;

    this._availableLanguages.removeAttribute("accesskey");
  },

  addLanguage() {
    var selectedID = this._availableLanguages.selectedItem.id;
    var preference = Preferences.get("intl.accept_languages");
    var arrayOfPrefs = preference.value.toLowerCase().split(/\s*,\s*/);
    for (var i = 0; i < arrayOfPrefs.length; ++i) {
      if (arrayOfPrefs[i] == selectedID) {
        return;
      }
    }

    this._selectedItemID = selectedID;

    if (preference.value == "") {
      preference.value = selectedID;
    } else {
      arrayOfPrefs.unshift(selectedID);
      preference.value = arrayOfPrefs.join(",");
    }

    this._acceptLanguages[selectedID] = true;
    this._availableLanguages.selectedItem = null;

    // Rebuild the available list with the added item removed...
    this._buildAvailableLanguageList().catch(console.error);
  },

  removeLanguage() {
    // Build the new preference value string.
    var languagesArray = [];
    for (var i = 0; i < this._activeLanguages.childNodes.length; ++i) {
      var item = this._activeLanguages.childNodes[i];
      if (!item.selected) {
        languagesArray.push(item.id);
      } else {
        this._acceptLanguages[item.id] = false;
      }
    }
    var string = languagesArray.join(",");

    // Get the item to select after the remove operation completes.
    var selection = this._activeLanguages.selectedItems;
    var lastSelected = selection[selection.length - 1];
    var selectItem = lastSelected.nextSibling || lastSelected.previousSibling;
    selectItem = selectItem ? selectItem.id : null;

    this._selectedItemID = selectItem;

    // Update the preference and force a UI rebuild
    var preference = Preferences.get("intl.accept_languages");
    preference.value = string;

    this._buildAvailableLanguageList().catch(console.error);
  },

  _getLocaleName(localeCode) {
    if (!this._availableLanguagesList.length) {
      this._loadAvailableLanguages();
    }
    let languageName = "";
    for (var i = 0; i < this._availableLanguagesList.length; ++i) {
      if (localeCode == this._availableLanguagesList[i].code) {
        return this._availableLanguagesList[i].name;
      }
      // Try resolving the locale code without region code. Can't return
      // directly because there might be a perfect match later.
      if (localeCode.split("-")[0] == this._availableLanguagesList[i].code) {
        languageName = this._availableLanguagesList[i].name;
      }
    }

    return languageName;
  },

  moveUp() {
    var selectedItem = this._activeLanguages.selectedItems[0];
    var previousItem = selectedItem.previousSibling;

    var string = "";
    for (var i = 0; i < this._activeLanguages.childNodes.length; ++i) {
      var item = this._activeLanguages.childNodes[i];
      string += i == 0 ? "" : ",";
      if (item.id == previousItem.id) {
        string += selectedItem.id;
      } else if (item.id == selectedItem.id) {
        string += previousItem.id;
      } else {
        string += item.id;
      }
    }

    this._selectedItemID = selectedItem.id;

    // Update the preference and force a UI rebuild
    var preference = Preferences.get("intl.accept_languages");
    preference.value = string;
  },

  moveDown() {
    var selectedItem = this._activeLanguages.selectedItems[0];
    var nextItem = selectedItem.nextSibling;

    var string = "";
    for (var i = 0; i < this._activeLanguages.childNodes.length; ++i) {
      var item = this._activeLanguages.childNodes[i];
      string += i == 0 ? "" : ",";
      if (item.id == nextItem.id) {
        string += selectedItem.id;
      } else if (item.id == selectedItem.id) {
        string += nextItem.id;
      } else {
        string += item.id;
      }
    }

    this._selectedItemID = selectedItem.id;

    // Update the preference and force a UI rebuild
    var preference = Preferences.get("intl.accept_languages");
    preference.value = string;
  },

  onLanguageSelect() {
    var upButton = document.getElementById("up");
    var downButton = document.getElementById("down");
    var removeButton = document.getElementById("remove");
    switch (this._activeLanguages.selectedCount) {
      case 0:
        upButton.disabled = downButton.disabled = removeButton.disabled = true;
        break;
      case 1:
        upButton.disabled = this._activeLanguages.selectedIndex == 0;
        downButton.disabled =
          this._activeLanguages.selectedIndex ==
          this._activeLanguages.childNodes.length - 1;
        removeButton.disabled = false;
        break;
      default:
        upButton.disabled = true;
        downButton.disabled = true;
        removeButton.disabled = false;
    }
  },

  readSpoofEnglish() {
    var checkbox = document.getElementById("spoofEnglish");
    var resistFingerprinting = Services.prefs.getBoolPref(
      "privacy.resistFingerprinting"
    );
    if (!resistFingerprinting) {
      checkbox.hidden = true;
      return false;
    }

    var spoofEnglish = Preferences.get("privacy.spoof_english").value;
    var activeLanguages = this._activeLanguages;
    var availableLanguages = this._availableLanguages;
    checkbox.hidden = false;
    switch (spoofEnglish) {
      case 1: // don't spoof intl.accept_languages
        activeLanguages.disabled = false;
        activeLanguages.selectItem(activeLanguages.firstChild);
        availableLanguages.disabled = false;
        this.onAvailableLanguageSelect();
        return false;
      case 2: // spoof intl.accept_languages
        activeLanguages.clearSelection();
        activeLanguages.disabled = true;
        availableLanguages.disabled = true;
        this.onAvailableLanguageSelect();
        return true;
      default:
        // will prompt for spoofing intl.accept_languages if resisting fingerprinting
        return false;
    }
  },

  writeSpoofEnglish() {
    return document.getElementById("spoofEnglish").checked ? 2 : 1;
  },
};
