/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from ../../../toolkit/content/preferencesBindings.js */

var {Sanitizer} = ChromeUtils.import("resource:///modules/Sanitizer.jsm", {});

Preferences.addAll([
  { id: "privacy.cpd.history", type: "bool" },
  { id: "privacy.cpd.formdata", type: "bool" },
  { id: "privacy.cpd.downloads", type: "bool", disabled: true },
  { id: "privacy.cpd.cookies", type: "bool" },
  { id: "privacy.cpd.cache", type: "bool" },
  { id: "privacy.cpd.sessions", type: "bool" },
  { id: "privacy.cpd.offlineApps", type: "bool" },
  { id: "privacy.cpd.siteSettings", type: "bool" },
  { id: "privacy.sanitize.timeSpan", type: "int" },
]);

var gSanitizePromptDialog = {

  get bundleBrowser() {
    if (!this._bundleBrowser)
      this._bundleBrowser = document.getElementById("bundleBrowser");
    return this._bundleBrowser;
  },

  get selectedTimespan() {
    var durList = document.getElementById("sanitizeDurationChoice");
    return parseInt(durList.value);
  },

  get warningBox() {
    return document.getElementById("sanitizeEverythingWarningBox");
  },

  init() {
    // This is used by selectByTimespan() to determine if the window has loaded.
    this._inited = true;

    document.documentElement.getButton("accept").label =
      this.bundleBrowser.getString("sanitizeButtonOK");

    if (this.selectedTimespan === Sanitizer.TIMESPAN_EVERYTHING) {
      this.prepareWarning();
      this.warningBox.hidden = false;
      document.title =
        this.bundleBrowser.getString("sanitizeDialog2.everything.title");
    } else
      this.warningBox.hidden = true;
  },

  selectByTimespan() {
    // This method is the onselect handler for the duration dropdown.  As a
    // result it's called a couple of times before onload calls init().
    if (!this._inited)
      return;

    var warningBox = this.warningBox;

    // If clearing everything
    if (this.selectedTimespan === Sanitizer.TIMESPAN_EVERYTHING) {
      this.prepareWarning();
      if (warningBox.hidden) {
        warningBox.hidden = false;
        window.resizeBy(0, warningBox.boxObject.height);
      }
      window.document.title =
        this.bundleBrowser.getString("sanitizeDialog2.everything.title");
      return;
    }

    // If clearing a specific time range
    if (!warningBox.hidden) {
      window.resizeBy(0, -warningBox.boxObject.height);
      warningBox.hidden = true;
    }
    window.document.title =
      window.document.documentElement.getAttribute("noneverythingtitle");
  },

  sanitize() {
    // Update pref values before handing off to the sanitizer (bug 453440)
    this.updatePrefs();

    // As the sanitize is async, we disable the buttons, update the label on
    // the 'accept' button to indicate things are happening and return false -
    // once the async operation completes (either with or without errors)
    // we close the window.
    let docElt = document.documentElement;
    let acceptButton = docElt.getButton("accept");
    acceptButton.disabled = true;
    acceptButton.setAttribute("label",
                              this.bundleBrowser.getString("sanitizeButtonClearing"));
    docElt.getButton("cancel").disabled = true;

    try {
      let range = Sanitizer.getClearRange(this.selectedTimespan);
      let options = {
        ignoreTimespan: !range,
        range,
      };
      Sanitizer.sanitize(null, options)
        .catch(Cu.reportError)
        .then(() => window.close())
        .catch(Cu.reportError);
      return false;
    } catch (er) {
      Cu.reportError("Exception during sanitize: " + er);
      return true; // We *do* want to close immediately on error.
    }
  },

  /**
   * If the panel that displays a warning when the duration is "Everything" is
   * not set up, sets it up.  Otherwise does nothing.
   *
   * @param aDontShowItemList Whether only the warning message should be updated.
   *                          True means the item list visibility status should not
   *                          be changed.
   */
  prepareWarning(aDontShowItemList) {
    // If the date and time-aware locale warning string is ever used again,
    // initialize it here.  Currently we use the no-visits warning string,
    // which does not include date and time.  See bug 480169 comment 48.

    var warningStringID;
    if (this.hasNonSelectedItems()) {
      warningStringID = "sanitizeSelectedWarning";
      if (!aDontShowItemList)
        this.showItemList();
    } else {
      warningStringID = "sanitizeEverythingWarning2";
    }

    var warningDesc = document.getElementById("sanitizeEverythingWarning");
    warningDesc.textContent =
      this.bundleBrowser.getString(warningStringID);
  },

  /**
   * Return the boolean prefs that enable/disable clearing of various kinds
   * of history.  The only pref this excludes is privacy.sanitize.timeSpan.
   */
  _getItemPrefs() {
    return Preferences.getAll().filter(p => p.id !== "privacy.sanitize.timeSpan");
  },

  /**
   * Called when the value of a preference element is synced from the actual
   * pref.  Enables or disables the OK button appropriately.
   */
  onReadGeneric() {
    // Find any other pref that's checked and enabled (except for
    // privacy.sanitize.timeSpan, which doesn't affect the button's status).
    var found = this._getItemPrefs().some(pref => !!pref.value && !pref.disabled);

    try {
      document.documentElement.getButton("accept").disabled = !found;
    } catch (e) { }

    // Update the warning prompt if needed
    this.prepareWarning(true);

    return undefined;
  },

  /**
   * Sanitizer.prototype.sanitize() requires the prefs to be up-to-date.
   * Because the type of this prefwindow is "child" -- and that's needed because
   * without it the dialog has no OK and Cancel buttons -- the prefs are not
   * updated on dialogaccept.  We must therefore manually set the prefs
   * from their corresponding preference elements.
   */
  updatePrefs() {
    Services.prefs.setIntPref(Sanitizer.PREF_TIMESPAN, this.selectedTimespan);

    // Keep the pref for the download history in sync with the history pref.
    Preferences.get("privacy.cpd.downloads").value =
      Preferences.get("privacy.cpd.history").value;

    // Now manually set the prefs from their corresponding preference
    // elements.
    var prefs = this._getItemPrefs();
    for (let i = 0; i < prefs.length; ++i) {
      var p = prefs[i];
      Services.prefs.setBoolPref(p.name, p.value);
    }
  },

  /**
   * Check if all of the history items have been selected like the default status.
   */
  hasNonSelectedItems() {
    let checkboxes = document.querySelectorAll("checkbox[preference]");
    for (let i = 0; i < checkboxes.length; ++i) {
      let pref = Preferences.get(checkboxes[i].getAttribute("preference"));
      if (!pref.value)
        return true;
    }
    return false;
  },

  /**
   * Show the history items list.
   */
  showItemList() {
    var itemList = document.getElementById("itemList");
    var expanderButton = document.getElementById("detailsExpander");

    if (itemList.collapsed) {
      expanderButton.className = "expander-up";
      itemList.setAttribute("collapsed", "false");
      if (document.documentElement.boxObject.height)
        window.resizeBy(0, itemList.boxObject.height);
    }
  },

  /**
   * Hide the history items list.
   */
  hideItemList() {
    var itemList = document.getElementById("itemList");
    var expanderButton = document.getElementById("detailsExpander");

    if (!itemList.collapsed) {
      expanderButton.className = "expander-down";
      window.resizeBy(0, -itemList.boxObject.height);
      itemList.setAttribute("collapsed", "true");
    }
  },

  /**
   * Called by the item list expander button to toggle the list's visibility.
   */
  toggleItemList() {
    var itemList = document.getElementById("itemList");

    if (itemList.collapsed)
      this.showItemList();
    else
      this.hideItemList();
  }


};
