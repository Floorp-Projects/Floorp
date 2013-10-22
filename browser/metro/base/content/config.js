/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let Ci = Components.interfaces;

Components.utils.import("resource://gre/modules/Services.jsm");

var ViewConfig = {
  get _main() {
    delete this._main;
    return this._main = document.getElementById("main-container");
  },

  get _container() {
    delete this._container;
    return this._container = document.getElementById("prefs-container");
  },

  get _editor() {
    delete this._editor;
    return this._editor = document.getElementById("editor");
  },

  init: function init() {
    this._main.addEventListener("click", this, false);
    window.addEventListener("resize", this, false);
    window.addEventListener("prefchange", this, false);
    window.addEventListener("prefnew", this, false);

    this._handleWindowResize();
    this.filter("");

    document.getElementById("textbox").focus();
  },

  uninit: function uninit() {
    this._main.removeEventListener("click", this, false);
    window.removeEventListener("resize", this, false);
    window.removeEventListener("prefchange", this, false);
    window.removeEventListener("prefnew", this, false);
  },

  filter: function filter(aValue) {
    let row = document.getElementById("editor-row");

    let container = this._container;
    container.scrollBoxObject.scrollTo(0, 0);
    // Clear the list by replacing with a shallow copy
    let empty = container.cloneNode(false);
    empty.appendChild(row);
    container.parentNode.replaceChild(empty, container);
    this._container = empty;

    let result = Utils.getPrefs(aValue);
    this._container.setItems(result.map(this._createItem, this));
  },

  open: function open(aType) {
    let buttons = document.getElementById("editor-buttons-add");
    buttons.setAttribute("hidden", "true");

    let shouldFocus = false;
    let setting = document.getElementById("editor-setting");
    switch (aType) {
      case Ci.nsIPrefBranch.PREF_INT:
        setting.setAttribute("type", "integer");
        setting.setAttribute("min", -Infinity);
        break;
      case Ci.nsIPrefBranch.PREF_BOOL:
        setting.setAttribute("type", "bool");
        break;
      case Ci.nsIPrefBranch.PREF_STRING:
        setting.setAttribute("type", "string");
        break;
    }

    setting.removeAttribute("title");
    setting.removeAttribute("pref");
    if (setting.input)
      setting.input.value = "";

    document.getElementById("editor-container").appendChild(this._editor);
    let nameField = document.getElementById("editor-name");
    nameField.value = "";

    this._editor.setAttribute("hidden", "false");
    this._currentItem = null;
    nameField.focus();
  },

  close: function close(aValid) {
    this._editor.setAttribute("hidden", "true");
    let buttons = document.getElementById("editor-buttons-add");
    buttons.setAttribute("hidden", "false");

    if (aValid) {
      let name = document.getElementById("editor-name").inputField.value;
      if (name != "") {
        let setting = document.getElementById("editor-setting");
        setting.setAttribute("pref", name);
        setting.valueToPreference();
      }
    }

    document.getElementById("editor-container").appendChild(this._editor);
  },

  _currentItem: null,

  delayEdit: function(aItem) {
    setTimeout(this.edit.bind(this), 0, aItem);
  },

  edit: function(aItem) {
    if (!aItem)
      return;

    let pref = Utils.getPref(aItem.getAttribute("name"));
    if (pref.lock || !pref.name || aItem == this._currentItem)
      return;

    this.close(false);
    this._currentItem = aItem;

    let setting = document.getElementById("editor-setting");
    let shouldFocus = false;
    switch (pref.type) {
      case Ci.nsIPrefBranch.PREF_BOOL:
        setting.setAttribute("type", "bool");
        break;

      case Ci.nsIPrefBranch.PREF_INT:
        setting.setAttribute("type", "integer");
        setting.setAttribute("increment", this.getIncrementForValue(pref.value));
        setting.setAttribute("min", -Infinity);
        shouldFocus = true;
        break;

      case Ci.nsIPrefBranch.PREF_STRING:
        setting.setAttribute("type", "string");
        shouldFocus = true;
        break;
    }

    setting.setAttribute("title", pref.name);
    setting.setAttribute("pref", pref.name);

    this._container.insertBefore(this._editor, aItem);

    let resetButton = document.getElementById("editor-reset");
    resetButton.setAttribute("disabled", pref.default);

    this._editor.setAttribute("default", pref.default);
    this._editor.setAttribute("hidden", "false");

    if (shouldFocus && setting.input)
      setting.input.focus();
  },

  reset: function reset(aItem) {
    let setting = document.getElementById("editor-setting");
    let pref = Utils.getPref(setting.getAttribute("pref"));
    if (!pref.default)
      Utils.resetPref(pref.name);
  },

  handleEvent: function handleEvent(aEvent) {
    switch (aEvent.type) {
      case "resize":
        this._handleWindowResize();
        break;

      case "prefchange":
      case "prefnew":
        this._handlePrefChange(aEvent.detail, aEvent.type == "prefnew");
        break;

      case "click":
        this._onClick();
        break;
    }
  },

  _handleWindowResize: function _handleWindowResize() {
    let mainBox = document.getElementById("main-container");
    let textbox = document.getElementById("textbox");
    let height = window.innerHeight - textbox.getBoundingClientRect().height;

    mainBox.setAttribute("height", height);
  },

  _onClick: function () {
    // Blur the search box when tapping anywhere else in the content
    // in order to close the soft keyboard.
    document.getElementById("textbox").blur();
  },

  _handlePrefChange: function _handlePrefChange(aIndex, aNew) {
    let isEditing = !this._editor.hidden;
    let shouldUpdateEditor = false;
    if (isEditing) {
      let setting = document.getElementById("editor-setting");
      let editorIndex = Utils.getPrefIndex(setting.getAttribute("pref"));
      shouldUpdateEditor = (aIndex == editorIndex);
      if(shouldUpdateEditor || aIndex > editorIndex)
        aIndex += 1;
    }

    // XXX An item display value will probably fail if a pref is changed in the
    //     background while there is a filter on the pref
    let item = shouldUpdateEditor ? this._editor.nextSibling
                                  : this._container.childNodes[aIndex + 1];// add 1 because of the new pref row
    if (!item) // the pref is not viewable
      return;

    if (aNew) {
      let pref = Utils.getPrefByIndex(aIndex);
      let row = this._createItem(pref);
      this._container.insertBefore(row, item);
      return;
    }

    let pref = Utils.getPref(item.getAttribute("name"));
    if (shouldUpdateEditor) {
      this._editor.setAttribute("default", pref.default);

      let resetButton = document.getElementById("editor-reset");
      resetButton.disabled = pref.default;
    }

    item.setAttribute("default", pref.default);
    item.lastChild.setAttribute("value", pref.value);
  },

  _createItem: function _createItem(aPref) {
    let row = document.createElement("richlistitem");

    row.setAttribute("name", aPref.name);
    row.setAttribute("type", aPref.type);
    row.setAttribute("role", "button");
    row.setAttribute("default", aPref.default);

    let label = document.createElement("label");
    label.setAttribute("class", "preferences-title");
    label.setAttribute("value", aPref.name);
    label.setAttribute("crop", "end");
    row.appendChild(label);

    label = document.createElement("label");
    label.setAttribute("class", "preferences-value");
    label.setAttribute("value", aPref.value);
    label.setAttribute("crop", "end");
    row.appendChild(label);

    return row;
  },

  getIncrementForValue: function getIncrementForValue(aValue) {
    let count = 1;
    while (aValue >= 100) {
      aValue /= 10;
      count *= 10;
    }
    return count;
  }
};

var Utils = {
  QueryInterface: function(aIID) {
    if (!aIID.equals(Ci.nsIObserver) && !aIID.equals(Ci.nsISupportsWeakReference))
      throw Components.results.NS_ERROR_NO_INTERFACE;
    return this;
  },

  get _branch() {
    delete this._branch;
    this._branch = Services.prefs.getBranch(null);
    this._branch.addObserver("", this, true);
    return this._branch;
  },

  get _preferences() {
    delete this._preferences;
    let list = this._branch.getChildList("", {}).filter(function(element) {
      return !(/^capability\./.test(element));
    });
    return this._preferences = list.sort().map(this.getPref, this);
  },

  getPrefs: function getPrefs(aValue) {
    let result = this._preferences.slice();;
    if (aValue != "") {
      let reg = this._generateRegexp(aValue);
      if (!reg)
        return [];

      result = this._preferences.filter(function(element, index, array) {
        return reg.test(element.name + ";" + element.value);
      });
    }

    return result;
  },

  getPref: function getPref(aPrefName) {
    let branch = this._branch;
    let pref = {
                 name: aPrefName,
                 value:  "",
                 default: !branch.prefHasUserValue(aPrefName),
                 lock: branch.prefIsLocked(aPrefName),
                 type: branch.getPrefType(aPrefName)
               };

    try {
      switch (pref.type) {
        case Ci.nsIPrefBranch.PREF_BOOL:
          pref.value = branch.getBoolPref(aPrefName).toString();
          break;
        case Ci.nsIPrefBranch.PREF_INT:
          pref.value = branch.getIntPref(aPrefName).toString();
          break;
        default:
        case Ci.nsIPrefBranch.PREF_STRING:
          pref.value = branch.getComplexValue(aPrefName, Ci.nsISupportsString).data;
          // Try in case it's a localized string (will throw an exception if not)
          if (pref.default && /^chrome:\/\/.+\/locale\/.+\.properties/.test(pref.value))
            pref.value = branch.getComplexValue(aPrefName, Ci.nsIPrefLocalizedString).data;
          break;
      }
    } catch (e) {}

    return pref;
  },

  getPrefByIndex: function getPrefByIndex(aIndex) {
    return this._preferences[aIndex];
  },

  getPrefIndex: function getPrefIndex(aPrefName) {
    let prefs = this._preferences;
    let high = prefs.length - 1;
    let low = 0, middle, element;

    while (low <= high) {
      middle = parseInt((low + high) / 2);
      element = prefs[middle];

      if (element.name > aPrefName)
        high = middle - 1;
      else if (element.name < aPrefName)
        low = middle + 1;
      else
        return middle;
    }

    return -1;
  },

  resetPref: function resetPref(aPrefName) {
    this._branch.clearUserPref(aPrefName);
  },

  observe: function observe(aSubject, aTopic, aPrefName) {
    if (aTopic != "nsPref:changed" || /^capability\./.test(aPrefName)) // avoid displaying "private" preferences
      return;

    let type = "prefchange";
    let index = this.getPrefIndex(aPrefName);
    if (index != - 1) {
      // update the inner array
      let pref = this.getPref(aPrefName);
      this._preferences[index].value = pref.value;
    }
    else {
      // XXX we could do better here
      let list = this._branch.getChildList("", {}).filter(function(element, index, array) {
        return !(/^capability\./.test(element));
      });
      this._preferences = list.sort().map(this.getPref, this);

      type = "prefnew";
      index = this.getPrefIndex(aPrefName);
    }

    let evt = document.createEvent("UIEvents");
    evt.initUIEvent(type, true, true, window, index);
    window.dispatchEvent(evt);
  },

  _generateRegexp: function _generateRegexp(aValue) {
    if (aValue.charAt(0) == "/") {
      try {
        let rv = aValue.match(/^\/(.*)\/(i?)$/);
        return RegExp(rv[1], rv[2]);
      }
      catch (e) {
        return null; // Do nothing on incomplete or bad RegExp
      }
    }

    return RegExp(aValue.replace(/([^* \w])/g, "\\$1").replace(/^\*+/, "")
                        .replace(/\*+/g, ".*"), "i");
  }
};

