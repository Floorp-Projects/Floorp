/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { DeferredTask } = ChromeUtils.import(
  "resource://gre/modules/DeferredTask.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { Preferences } = ChromeUtils.import(
  "resource://gre/modules/Preferences.jsm"
);

const SEARCH_TIMEOUT_MS = 100;
const SEARCH_AUTO_MIN_CRARACTERS = 3;

const GETTERS_BY_PREF_TYPE = {
  [Ci.nsIPrefBranch.PREF_BOOL]: "getBoolPref",
  [Ci.nsIPrefBranch.PREF_INT]: "getIntPref",
  [Ci.nsIPrefBranch.PREF_STRING]: "getStringPref",
};

const STRINGS_ADD_BY_TYPE = {
  Boolean: "about-config-pref-add-type-boolean",
  Number: "about-config-pref-add-type-number",
  String: "about-config-pref-add-type-string",
};

let gDefaultBranch = Services.prefs.getDefaultBranch("");
let gFilterPrefsTask = new DeferredTask(
  () => filterPrefs(),
  SEARCH_TIMEOUT_MS,
  0
);

/**
 * Maps the name of each preference in the back-end to its PrefRow object,
 * separating the preferences that actually exist. This is as an optimization to
 * avoid querying the preferences service each time the list is filtered.
 */
let gExistingPrefs = new Map();
let gDeletedPrefs = new Map();

/**
 * Also cache several values to improve the performance of common use cases.
 */
let gSortedExistingPrefs = null;
let gSearchInput = null;
let gPrefsTable = null;

/**
 * Reference to the PrefRow currently being edited, if any.
 */
let gPrefInEdit = null;

/**
 * Lowercase substring that should be contained in the preference name.
 */
let gFilterString = null;

/**
 * RegExp that should be matched to the preference name.
 */
let gFilterPattern = null;

/**
 * True if we were requested to show all preferences.
 */
let gFilterShowAll = false;

class PrefRow {
  constructor(name) {
    this.name = name;
    this.value = true;
    this.hidden = false;
    this.odd = false;
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
      if (gExistingPrefs.has(this.name)) {
        gExistingPrefs.delete(this.name);
        gSortedExistingPrefs = null;
      }
      gDeletedPrefs.set(this.name, this);
      return;
    }

    if (!gExistingPrefs.has(this.name)) {
      gExistingPrefs.set(this.name, this);
      gSortedExistingPrefs = null;
    }
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
        this.value = Services.prefs.getComplexValue(
          this.name,
          Ci.nsIPrefLocalizedString
        ).data;
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
    return (
      gFilterShowAll ||
      (gFilterPattern && gFilterPattern.test(this.name)) ||
      (gFilterString && this.name.toLowerCase().includes(gFilterString))
    );
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
    this._element._pref = this;

    let nameCell = document.createElement("th");
    this._element.append(
      nameCell,
      (this.valueCell = document.createElement("td")),
      (this.editCell = document.createElement("td")),
      (this.resetCell = document.createElement("td"))
    );
    this.editCell.appendChild(
      (this.editButton = document.createElement("button"))
    );
    delete this.resetButton;

    nameCell.setAttribute("scope", "row");
    this.valueCell.className = "cell-value";
    this.editCell.className = "cell-edit";
    this.resetCell.className = "cell-reset";

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
      document.l10n.setAttributes(outerSpan, spanL10nId, {
        value: "" + this.value,
      });
      outerSpan.appendChild(span);
      this.valueCell.textContent = "";
      this.valueCell.append(outerSpan);
      if (this.type == "Boolean") {
        document.l10n.setAttributes(
          this.editButton,
          "about-config-pref-toggle"
        );
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
          radio.id = `add-type-${type}`;
          radio.checked = this.type == type;
          form.appendChild(radio);
          let radioLabel = document.createElement("label");
          radioLabel.setAttribute("for", radio.id);
          document.l10n.setAttributes(radioLabel, STRINGS_ADD_BY_TYPE[type]);
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
        document.l10n.setAttributes(
          this.resetButton,
          "about-config-pref-delete"
        );
        this.resetButton.className = "";
      } else {
        document.l10n.setAttributes(
          this.resetButton,
          "about-config-pref-reset"
        );
        this.resetButton.className = "button-reset";
      }
    } else if (this.resetButton) {
      this.resetButton.remove();
      delete this.resetButton;
    }

    this.refreshClass();
  }

  refreshClass() {
    if (!this._element) {
      // No need to update if this preference was never added to the table.
      return;
    }

    let className;
    if (this.hidden) {
      className = "hidden";
    } else {
      className =
        (this.hasUserValue ? "has-user-value " : "") +
        (this.isLocked ? "locked " : "") +
        (this.exists ? "" : "deleted ") +
        (this.odd ? "odd " : "");
    }

    if (this._lastClassName !== className) {
      this._element.className = this._lastClassName = className;
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
  window.addEventListener(
    "load",
    () => {
      if (document.getElementById("about-config-search").value) {
        filterPrefs();
      }
    },
    { once: true }
  );
}

function onWarningButtonClick() {
  Services.prefs.setBoolPref(
    "browser.aboutConfig.showWarning",
    document.getElementById("showWarningNextTime").checked
  );
  loadPrefs();
}

function loadPrefs() {
  document.body.className = "config-background";
  [...document.styleSheets].find(s => s.title == "infop").disabled = true;

  let { content } = document.getElementById("main");
  document.body.textContent = "";
  document.body.appendChild(content);

  let search = (gSearchInput = document.getElementById("about-config-search"));
  let prefs = (gPrefsTable = document.getElementById("prefs"));
  let showAll = document.getElementById("show-all");
  search.focus();

  for (let name of Services.prefs.getChildList("")) {
    new PrefRow(name);
  }

  search.addEventListener("keypress", event => {
    if (event.key == "Escape") {
      // The ESC key returns immediately to the initial empty page.
      search.value = "";
      gFilterPrefsTask.disarm();
      filterPrefs();
    } else if (event.key == "Enter") {
      // The Enter key filters immediately even if the search string is short.
      gFilterPrefsTask.disarm();
      filterPrefs({ shortString: true });
    }
  });

  search.addEventListener("input", () => {
    // We call "disarm" to restart the timer at every input.
    gFilterPrefsTask.disarm();
    if (search.value.trim().length < SEARCH_AUTO_MIN_CRARACTERS) {
      // Return immediately to the empty page if the search string is short.
      filterPrefs();
    } else {
      gFilterPrefsTask.arm();
    }
  });

  showAll.addEventListener("click", event => {
    search.focus();
    search.value = "";
    gFilterPrefsTask.disarm();
    filterPrefs({ showAll: true });
  });

  prefs.addEventListener("click", event => {
    if (event.target.localName != "button") {
      return;
    }
    let pref = event.target.closest("tr")._pref;
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

function filterPrefs(options = {}) {
  if (gPrefInEdit) {
    gPrefInEdit.endEdit();
  }
  gDeletedPrefs.clear();

  let searchName = gSearchInput.value.trim();
  if (searchName.length < SEARCH_AUTO_MIN_CRARACTERS && !options.shortString) {
    searchName = "";
  }

  gFilterString = searchName.toLowerCase();
  gFilterShowAll = !!options.showAll;

  gFilterPattern = null;
  if (gFilterString.includes("*")) {
    gFilterPattern = new RegExp(gFilterString.replace(/\*+/g, ".*"), "i");
    gFilterString = "";
  }

  let showResults = gFilterString || gFilterPattern || gFilterShowAll;
  document.getElementById("show-all").classList.toggle("hidden", showResults);

  let prefArray = [];
  if (showResults) {
    if (!gSortedExistingPrefs) {
      gSortedExistingPrefs = [...gExistingPrefs.values()];
      gSortedExistingPrefs.sort((a, b) => a.name > b.name);
    }
    prefArray = gSortedExistingPrefs;
  }

  // The slowest operations tend to be the addition and removal of DOM nodes, so
  // this algorithm tries to reduce removals by hiding nodes instead. This
  // happens frequently when the set narrows while typing preference names. We
  // iterate the nodes already in the table in parallel to those we want to
  // show, because the two lists are sorted and they will often match already.
  let fragment = null;
  let indexInArray = 0;
  let elementInTable = gPrefsTable.firstElementChild;
  let odd = false;
  while (indexInArray < prefArray.length || elementInTable) {
    // For efficiency, filter the array while we are iterating.
    let prefInArray = prefArray[indexInArray];
    if (prefInArray) {
      if (!prefInArray.matchesFilter) {
        indexInArray++;
        continue;
      }
      prefInArray.hidden = false;
      prefInArray.odd = odd;
    }

    let prefInTable = elementInTable && elementInTable._pref;
    if (!prefInTable) {
      // We're at the end of the table, we just have to insert all the matching
      // elements that remain in the array. We can use a fragment to make the
      // insertions faster, which is useful during the initial filtering.
      if (!fragment) {
        fragment = document.createDocumentFragment();
      }
      fragment.appendChild(prefInArray.getElement());
    } else if (prefInTable == prefInArray) {
      // We got two matching elements, we just need to update the visibility.
      elementInTable = elementInTable.nextElementSibling;
    } else if (prefInArray && prefInArray.name < prefInTable.name) {
      // The iteration in the table is ahead of the iteration in the array.
      // Insert or move the array element, and advance the array index.
      gPrefsTable.insertBefore(prefInArray.getElement(), elementInTable);
    } else {
      // The iteration in the array is ahead of the iteration in the table.
      // Hide the element in the table, and advance to the next element.
      let nextElementInTable = elementInTable.nextElementSibling;
      if (!prefInTable.exists) {
        // Remove rows for deleted preferences, or temporary addition rows.
        elementInTable.remove();
      } else {
        // Keep the element for the next filtering if the preference exists.
        prefInTable.hidden = true;
        prefInTable.refreshClass();
      }
      elementInTable = nextElementInTable;
      continue;
    }

    prefInArray.refreshClass();
    odd = !odd;
    indexInArray++;
  }

  if (fragment) {
    gPrefsTable.appendChild(fragment);
  }

  if (searchName && !gExistingPrefs.has(searchName)) {
    gPrefsTable.appendChild(new PrefRow(searchName).getElement());
  }

  // We only start observing preference changes after the first search is done,
  // so that newly added preferences won't appear while the page is still empty.
  if (!gPrefObserverRegistered) {
    gPrefObserverRegistered = true;
    Services.prefs.addObserver("", gPrefObserver);
    window.addEventListener(
      "unload",
      () => {
        Services.prefs.removeObserver("", gPrefObserver);
      },
      { once: true }
    );
  }

  document.body.classList.toggle(
    "config-warning",
    location.href.split(":").every(l => gFilterString.includes(l))
  );
}
