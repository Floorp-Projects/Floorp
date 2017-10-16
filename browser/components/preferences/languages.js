/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var gLanguagesDialog = {

  _availableLanguagesList: [],
  _acceptLanguages: { },

  _selectedItemID: null,

  init() {
    if (!this._availableLanguagesList.length)
      this._loadAvailableLanguages();
  },

  // Ugly hack used to trigger extra reflow in order to work around XUL bug 1194844;
  // see bug 1194346.
  forceReflow() {
    this._activeLanguages.style.fontKerning = "none";
    setTimeout(() => {
      this._activeLanguages.style.removeProperty("font-kerning");
    }, 0);
  },

  get _activeLanguages() {
    return document.getElementById("activeLanguages");
  },

  get _availableLanguages() {
    return document.getElementById("availableLanguages");
  },

  _loadAvailableLanguages() {
    // This is a parser for: resource://gre/res/language.properties
    // The file is formatted like so:
    // ab[-cd].accept=true|false
    //  ab = language
    //  cd = region
    var bundleAccepted    = document.getElementById("bundleAccepted");
    var bundleRegions     = document.getElementById("bundleRegions");
    var bundleLanguages   = document.getElementById("bundleLanguages");
    var bundlePreferences = document.getElementById("bundlePreferences");

    function LanguageInfo(aName, aABCD, aIsVisible) {
      this.name = aName;
      this.abcd = aABCD;
      this.isVisible = aIsVisible;
    }

    // 1) Read the available languages out of language.properties
    var strings = bundleAccepted.strings;
    while (strings.hasMoreElements()) {
      var currString = strings.getNext();
      if (!(currString instanceof Components.interfaces.nsIPropertyElement))
        break;

      var property = currString.key.split("."); // ab[-cd].accept
      if (property[1] == "accept") {
        var abCD = property[0];
        var abCDPairs = abCD.split("-");      // ab[-cd]
        var useABCDFormat = abCDPairs.length > 1;
        var ab = useABCDFormat ? abCDPairs[0] : abCD;
        var cd = useABCDFormat ? abCDPairs[1] : "";
        if (ab) {
          var language = "";
          try {
            language = bundleLanguages.getString(ab);
          } catch (e) { continue; }

          var region = "";
          if (useABCDFormat) {
            try {
              region = bundleRegions.getString(cd);
            } catch (e) { continue; }
          }

          var name = "";
          if (useABCDFormat)
            name = bundlePreferences.getFormattedString("languageRegionCodeFormat",
                                                        [language, region, abCD]);
          else
            name = bundlePreferences.getFormattedString("languageCodeFormat",
                                                        [language, abCD]);

          if (name && abCD) {
            var isVisible = currString.value == "true" &&
                            (!(abCD in this._acceptLanguages) || !this._acceptLanguages[abCD]);
            var li = new LanguageInfo(name, abCD, isVisible);
            this._availableLanguagesList.push(li);
          }
        }
      }
    }
    this._buildAvailableLanguageList();
  },

  _buildAvailableLanguageList() {
    var availableLanguagesPopup = document.getElementById("availableLanguagesPopup");
    while (availableLanguagesPopup.hasChildNodes())
      availableLanguagesPopup.firstChild.remove();

    // Sort the list of languages by name
    this._availableLanguagesList.sort(function(a, b) {
                                        return a.name.localeCompare(b.name);
                                      });

    // Load the UI with the data
    for (var i = 0; i < this._availableLanguagesList.length; ++i) {
      var abCD = this._availableLanguagesList[i].abcd;
      if (this._availableLanguagesList[i].isVisible &&
          (!(abCD in this._acceptLanguages) || !this._acceptLanguages[abCD])) {
        var menuitem = document.createElement("menuitem");
        menuitem.id = this._availableLanguagesList[i].abcd;
        availableLanguagesPopup.appendChild(menuitem);
        menuitem.setAttribute("label", this._availableLanguagesList[i].name);
      }
    }
  },

  readAcceptLanguages() {
    while (this._activeLanguages.hasChildNodes())
      this._activeLanguages.firstChild.remove();

    var selectedIndex = 0;
    var preference = document.getElementById("intl.accept_languages");
    if (preference.value == "")
      return undefined;
    var languages = preference.value.toLowerCase().split(/\s*,\s*/);
    for (var i = 0; i < languages.length; ++i) {
      var name = this._getLanguageName(languages[i]);
      if (!name)
        name = "[" + languages[i] + "]";
      var listitem = document.createElement("listitem");
      listitem.id = languages[i];
      if (languages[i] == this._selectedItemID)
        selectedIndex = i;
      this._activeLanguages.appendChild(listitem);
      listitem.setAttribute("label", name);

      // Hash this language as an "Active" language so we don't
      // show it in the list that can be added.
      this._acceptLanguages[languages[i]] = true;
    }

    if (this._activeLanguages.childNodes.length > 0) {
      this._activeLanguages.ensureIndexIsVisible(selectedIndex);
      this._activeLanguages.selectedIndex = selectedIndex;
    }

    return undefined;
  },

  writeAcceptLanguages() {
    return undefined;
  },

  onAvailableLanguageSelect() {
    var addButton = document.getElementById("addButton");
    addButton.disabled = false;

    this._availableLanguages.removeAttribute("accesskey");
  },

  addLanguage() {
    var selectedID = this._availableLanguages.selectedItem.id;
    var preference = document.getElementById("intl.accept_languages");
    var arrayOfPrefs = preference.value.toLowerCase().split(/\s*,\s*/);
    for (var i = 0; i < arrayOfPrefs.length; ++i ) {
      if (arrayOfPrefs[i] == selectedID)
        return;
    }

    this._selectedItemID = selectedID;

    if (preference.value == "")
      preference.value = selectedID;
    else {
      arrayOfPrefs.unshift(selectedID);
      preference.value = arrayOfPrefs.join(",");
    }

    this._acceptLanguages[selectedID] = true;
    this._availableLanguages.selectedItem = null;

    // Rebuild the available list with the added item removed...
    this._buildAvailableLanguageList();

    this._availableLanguages.setAttribute("label", this._availableLanguages.getAttribute("label2"));
  },

  removeLanguage() {
    // Build the new preference value string.
    var languagesArray = [];
    for (var i = 0; i < this._activeLanguages.childNodes.length; ++i) {
      var item = this._activeLanguages.childNodes[i];
      if (!item.selected)
        languagesArray.push(item.id);
      else
        this._acceptLanguages[item.id] = false;
    }
    var string = languagesArray.join(",");

    // Get the item to select after the remove operation completes.
    var selection = this._activeLanguages.selectedItems;
    var lastSelected = selection[selection.length - 1];
    var selectItem = lastSelected.nextSibling || lastSelected.previousSibling;
    selectItem = selectItem ? selectItem.id : null;

    this._selectedItemID = selectItem;

    // Update the preference and force a UI rebuild
    var preference = document.getElementById("intl.accept_languages");
    preference.value = string;

    this._buildAvailableLanguageList();
  },

  _getLanguageName(aABCD) {
    if (!this._availableLanguagesList.length)
      this._loadAvailableLanguages();
    for (var i = 0; i < this._availableLanguagesList.length; ++i) {
      if (aABCD == this._availableLanguagesList[i].abcd)
        return this._availableLanguagesList[i].name;
    }
    return "";
  },

  moveUp() {
    var selectedItem = this._activeLanguages.selectedItems[0];
    var previousItem = selectedItem.previousSibling;

    var string = "";
    for (var i = 0; i < this._activeLanguages.childNodes.length; ++i) {
      var item = this._activeLanguages.childNodes[i];
      string += (i == 0 ? "" : ",");
      if (item.id == previousItem.id)
        string += selectedItem.id;
      else if (item.id == selectedItem.id)
        string += previousItem.id;
      else
        string += item.id;
    }

    this._selectedItemID = selectedItem.id;

    // Update the preference and force a UI rebuild
    var preference = document.getElementById("intl.accept_languages");
    preference.value = string;
  },

  moveDown() {
    var selectedItem = this._activeLanguages.selectedItems[0];
    var nextItem = selectedItem.nextSibling;

    var string = "";
    for (var i = 0; i < this._activeLanguages.childNodes.length; ++i) {
      var item = this._activeLanguages.childNodes[i];
      string += (i == 0 ? "" : ",");
      if (item.id == nextItem.id)
        string += selectedItem.id;
      else if (item.id == selectedItem.id)
        string += nextItem.id;
      else
        string += item.id;
    }

    this._selectedItemID = selectedItem.id;

    // Update the preference and force a UI rebuild
    var preference = document.getElementById("intl.accept_languages");
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
      downButton.disabled = this._activeLanguages.selectedIndex == this._activeLanguages.childNodes.length - 1;
      removeButton.disabled = false;
      break;
    default:
      upButton.disabled = true;
      downButton.disabled = true;
      removeButton.disabled = false;
    }
  }
};

