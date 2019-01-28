/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.import("resource://gre/modules/DeferredTask.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/Preferences.jsm");

const SEARCH_TIMEOUT_MS = 500;

const GETTERS_BY_PREF_TYPE = {
  [Ci.nsIPrefBranch.PREF_BOOL]: "getBoolPref",
  [Ci.nsIPrefBranch.PREF_INT]: "getIntPref",
  [Ci.nsIPrefBranch.PREF_STRING]: "getStringPref",
};

let gDefaultBranch = Services.prefs.getDefaultBranch("");
let gFilterPrefsTask = new DeferredTask(() => filterPrefs(), SEARCH_TIMEOUT_MS);

/**
 * Maps the name of each preference in the back-end to its PrefRow object,
 * separating the preferences that actually exist. This is as an optimization to
 * avoid querying the preferences service each time the list is filtered.
 */
let gExistingPrefs = new Map();
let gDeletedPrefs = new Map();

/**
 * Maps each row element currently in the table to its PrefRow object.
 */
let gElementToPrefMap = new WeakMap();

/**
 * Reference to the PrefRow currently being edited, if any.
 */
let gPrefInEdit = null;

/**
 * Lowercase substring that should be contained in the preference name.
 */
let gFilterString = null;

class PrefRow {
  constructor(name) {
    this.name = name;
    this.value = true;
    this.editing = false;
    this.refreshValue();
  }

  refreshValue() {
    let prefType = Services.prefs.getPrefType(this.name);

    // If this preference has been deleted, we keep its last known value.
    if (prefType == Ci.nsIPrefBranch.PREF_INVALID) {
      this.hasDefaultValue = false;
      this.hasUserValue = false;
      this.isLocked = false;
      gExistingPrefs.delete(this.name);
      gDeletedPrefs.set(this.name, this);
      return;
    }

    gExistingPrefs.set(this.name, this);
    gDeletedPrefs.delete(this.name);

    try {
      this.value = gDefaultBranch[GETTERS_BY_PREF_TYPE[prefType]](this.name);
      this.hasDefaultValue = true;
    } catch (ex) {
      this.hasDefaultValue = false;
    }
    this.hasUserValue = Services.prefs.prefHasUserValue(this.name);
    this.isLocked = Services.prefs.prefIsLocked(this.name);

    try {
      if (this.hasUserValue) {
        // This can throw for locked preferences without a default value.
        this.value = Services.prefs[GETTERS_BY_PREF_TYPE[prefType]](this.name);
      } else if (/^chrome:\/\/.+\/locale\/.+\.properties/.test(this.value)) {
        // We don't know which preferences should be read using getComplexValue,
        // so we use a heuristic to determine if this is a localized preference.
        // This can throw if there is no value in the localized files.
        this.value = Services.prefs.getComplexValue(this.name,
          Ci.nsIPrefLocalizedString).data;
      }
    } catch (ex) {
      this.value = "";
    }
  }

  get type() {
    return this.value.constructor.name;
  }

  get exists() {
    return this.hasDefaultValue || this.hasUserValue;
  }

  get matchesFilter() {
    return !gFilterString || this.name.toLowerCase().includes(gFilterString);
  }

  /**
   * Returns a reference to the table row element to be added to the document,
   * constructing and initializing it the first time this method is called.
   */
  getElement() {
    if (this._element) {
      return this._element;
    }

    this._element = document.createElement("tr");
    gElementToPrefMap.set(this._element, this);

    let nameCell = document.createElement("th");
    this._element.append(
      nameCell,
      this.valueCell = document.createElement("td"),
      this.editCell = document.createElement("td"),
      this.resetCell = document.createElement("td")
    );
    this.editCell.appendChild(
      this.editButton = document.createElement("button")
    );
    delete this.resetButton;

    nameCell.setAttribute("scope", "row");
    this.valueCell.className = "cell-value";
    this.editCell.className = "cell-edit";

    // Add <wbr> behind dots to prevent line breaking in random mid-word places.
    let parts = this.name.split(".");
    for (let i = 0; i < parts.length - 1; i++) {
      nameCell.append(parts[i] + ".", document.createElement("wbr"));
    }
    nameCell.append(parts[parts.length - 1]);

    this.refreshElement();

    return this._element;
  }

  refreshElement() {
    if (!this._element) {
      // No need to update if this preference was never added to the table.
      return;
    }

    this._element.classList.toggle("has-user-value", !!this.hasUserValue);
    this._element.classList.toggle("locked", !!this.isLocked);
    this._element.classList.toggle("deleted", !this.exists);
    if (this.exists && !this.editing) {
      // We need to place the text inside a "span" element to ensure that the
      // text copied to the clipboard includes all whitespace.
      let span = document.createElement("span");
      span.textContent = this.value;
      // We additionally need to wrap this with another "span" element to convey
      // the state to screen readers without affecting the visual presentation.
      span.setAttribute("aria-hidden", "true");
      let outerSpan = document.createElement("span");
      let spanL10nId = this.hasUserValue
                       ? "about-config-pref-accessible-value-custom"
                       : "about-config-pref-accessible-value-default";
      document.l10n.setAttributes(outerSpan, spanL10nId,
                                  { value: "" + this.value });
      outerSpan.appendChild(span);
      this.valueCell.textContent = "";
      this.valueCell.append(outerSpan);
      if (this.type == "Boolean") {
        document.l10n.setAttributes(this.editButton, "about-config-pref-toggle");
        this.editButton.className = "button-toggle";
      } else {
        document.l10n.setAttributes(this.editButton, "about-config-pref-edit");
        this.editButton.className = "button-edit";
      }
      this.editButton.removeAttribute("form");
      delete this.inputField;
    } else {
      this.valueCell.textContent = "";
      // The form is needed for the validation report to appear, but we need to
      // prevent the associated button from reloading the page.
      let form = document.createElement("form");
      form.addEventListener("submit", event => event.preventDefault());
      form.id = "form-edit";
      if (this.editing) {
        this.inputField = document.createElement("input");
        this.inputField.value = this.value;
        if (this.type == "Number") {
          this.inputField.type = "number";
          this.inputField.required = true;
          this.inputField.min = -2147483648;
          this.inputField.max = 2147483647;
        } else {
          this.inputField.type = "text";
        }
        form.appendChild(this.inputField);
        document.l10n.setAttributes(this.editButton, "about-config-pref-save");
        this.editButton.className = "primary button-save";
      } else {
        delete this.inputField;
        for (let type of ["Boolean", "Number", "String"]) {
          let radio = document.createElement("input");
          radio.type = "radio";
          radio.name = "type";
          radio.value = type;
          radio.checked = this.type == type;
          form.appendChild(radio);
          let radioLabel = document.createElement("span");
          radioLabel.textContent = type;
          form.appendChild(radioLabel);
        }
        form.addEventListener("click", event => {
          if (event.target.name != "type") {
            return;
          }
          let type = event.target.value;
          if (this.type != type) {
            if (type == "Boolean") {
              this.value = true;
            } else if (type == "Number") {
              this.value = 0;
            } else {
              this.value = "";
            }
          }
        });
        document.l10n.setAttributes(this.editButton, "about-config-pref-add");
        this.editButton.className = "button-add";
      }
      this.valueCell.appendChild(form);
      this.editButton.setAttribute("form", "form-edit");
    }
    this.editButton.disabled = this.isLocked;
    if (!this.isLocked && this.hasUserValue) {
      if (!this.resetButton) {
        this.resetButton = document.createElement("button");
        this.resetCell.appendChild(this.resetButton);
      }
      if (!this.hasDefaultValue) {
        document.l10n.setAttributes(this.resetButton,
                                    "about-config-pref-delete");
        this.resetButton.className = "";
      } else {
        document.l10n.setAttributes(this.resetButton,
                                    "about-config-pref-reset");
        this.resetButton.className = "button-reset";
      }
    } else if (this.resetButton) {
      this.resetButton.remove();
      delete this.resetButton;
    }
  }

  edit() {
    if (gPrefInEdit) {
      gPrefInEdit.endEdit();
    }
    gPrefInEdit = this;
    this.editing = true;
    this.refreshElement();
    this.inputField.focus();
  }

  save() {
    if (this.type == "Number") {
      if (!this.inputField.reportValidity()) {
        return;
      }
      Services.prefs.setIntPref(this.name, parseInt(this.inputField.value));
    } else {
      Services.prefs.setStringPref(this.name, this.inputField.value);
    }
    this.refreshValue();
    this.endEdit();
    this.editButton.focus();
  }

  endEdit() {
    this.editing = false;
    this.refreshElement();
    gPrefInEdit = null;
  }
}

let gPrefObserverRegistered = false;
let gPrefObserver = {
  observe(subject, topic, data) {
    let pref = gExistingPrefs.get(data) || gDeletedPrefs.get(data);
    if (pref) {
      pref.refreshValue();
      if (!pref.editing) {
        pref.refreshElement();
      }
      return;
    }

    let newPref = new PrefRow(data);
    if (newPref.matchesFilter) {
      document.getElementById("prefs").appendChild(newPref.getElement());
    }
  },
};

if (!Preferences.get("browser.aboutConfig.showWarning")) {
  // When showing the filtered preferences directly, remove the warning elements
  // immediately to prevent flickering, but wait to filter the preferences until
  // the value of the textbox has been restored from previous sessions.
  document.addEventListener("DOMContentLoaded", loadPrefs, { once: true });
  window.addEventListener("load", () => {
    if (document.getElementById("search").value) {
      filterPrefs();
    }
  }, { once: true });
}

function onWarningButtonClick() {
  Services.prefs.setBoolPref("browser.aboutConfig.showWarning",
    document.getElementById("showWarningNextTime").checked);
  loadPrefs();
}

function loadPrefs() {
  document.body.className = "config-background";
  [...document.styleSheets].find(s => s.title == "infop").disabled = true;

  document.body.textContent = "";
  let searchContainer = document.createElement("div");
  searchContainer.id = "search-container";
  let search = document.createElement("input");
  search.type = "text";
  search.id = "search";
  document.l10n.setAttributes(search, "about-config-search");
  searchContainer.appendChild(search);
  document.body.appendChild(searchContainer);
  search.focus();

  let prefs = document.createElement("table");
  prefs.id = "prefs";
  document.body.appendChild(prefs);

  for (let name of Services.prefs.getChildList("")) {
    new PrefRow(name);
  }

  search.addEventListener("keypress", event => {
    switch (event.key) {
      case "Escape":
        search.value = "";
        // Fall through.
      case "Enter":
        gFilterPrefsTask.disarm();
        filterPrefs();
    }
  });

  search.addEventListener("input", () => {
    // We call "disarm" to restart the timer at every input.
    gFilterPrefsTask.disarm();
    gFilterPrefsTask.arm();
  });

  prefs.addEventListener("click", event => {
    if (event.target.localName != "button") {
      return;
    }
    let pref = gElementToPrefMap.get(event.target.closest("tr"));
    let button = event.target.closest("button");
    if (button.classList.contains("button-add")) {
      Preferences.set(pref.name, pref.value);
      if (pref.type != "Boolean") {
        pref.edit();
      }
    } else if (button.classList.contains("button-toggle")) {
      Services.prefs.setBoolPref(pref.name, !pref.value);
    } else if (button.classList.contains("button-edit")) {
      pref.edit();
    } else if (button.classList.contains("button-save")) {
      pref.save();
    } else {
      // This is "button-reset" or "button-delete".
      pref.editing = false;
      Services.prefs.clearUserPref(pref.name);
      pref.editButton.focus();
    }
  });
}

function filterPrefs() {
  if (gPrefInEdit) {
    gPrefInEdit.endEdit();
  }
  gDeletedPrefs.clear();

  let searchName = document.getElementById("search").value.trim();
  gFilterString = searchName.toLowerCase();
  let prefArray = [...gExistingPrefs.values()];
  if (gFilterString) {
    prefArray = prefArray.filter(pref => pref.matchesFilter);
  }
  prefArray.sort((a, b) => a.name > b.name);
  if (searchName && !gExistingPrefs.has(searchName)) {
    prefArray.push(new PrefRow(searchName));
  }

  let prefsElement = document.getElementById("prefs");
  prefsElement.textContent = "";
  let fragment = document.createDocumentFragment();
  for (let pref of prefArray) {
    fragment.appendChild(pref.getElement());
  }
  prefsElement.appendChild(fragment);

  // We only start observing preference changes after the first search is done,
  // so that newly added preferences won't appear while the page is still empty.
  if (!gPrefObserverRegistered) {
    gPrefObserverRegistered = true;
    Services.prefs.addObserver("", gPrefObserver);
    window.addEventListener("unload", () => {
      Services.prefs.removeObserver("", gPrefObserver);
    }, { once: true });
  }

  document.body.classList.toggle("config-warning",
    location.href.split(":").every(l => gFilterString.includes(l)));
}
