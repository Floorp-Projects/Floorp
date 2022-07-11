/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from /toolkit/content/preferencesBindings.js */

var { Sanitizer } = ChromeUtils.import("resource:///modules/Sanitizer.jsm");

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
  get selectedTimespan() {
    var durList = document.getElementById("sanitizeDurationChoice");
    return parseInt(durList.value);
  },

  get warningBox() {
    return document.getElementById("sanitizeEverythingWarningBox");
  },

  async init() {
    // This is used by selectByTimespan() to determine if the window has loaded.
    this._inited = true;
    this._dialog = document.querySelector("dialog");
    let arg = window.arguments?.[0] || {};
    if (arg.inBrowserWindow) {
      this._dialog.setAttribute("inbrowserwindow", "true");
      this._observeTitleForChanges();
    } else if (arg.wrappedJSObject?.needNativeUI) {
      document
        .getElementById("sanitizeDurationChoice")
        .setAttribute("native", "true");
      for (let cb of document.querySelectorAll("checkbox")) {
        cb.setAttribute("native", "true");
      }
    }

    let OKButton = this._dialog.getButton("accept");
    document.l10n.setAttributes(OKButton, "sanitize-button-ok");

    document.addEventListener("dialogaccept", function(e) {
      gSanitizePromptDialog.sanitize(e);
    });

    this.registerSyncFromPrefListeners();

    if (this.selectedTimespan === Sanitizer.TIMESPAN_EVERYTHING) {
      this.prepareWarning();
      this.warningBox.hidden = false;
      document.l10n.setAttributes(
        document.documentElement,
        "dialog-title-everything"
      );
      let warningDesc = document.getElementById("sanitizeEverythingWarning");
      // Ensure we've translated and sized the warning.
      await document.l10n.translateFragment(warningDesc);
      let rootWin = window.browsingContext.topChromeWindow;
      await rootWin.promiseDocumentFlushed(() => {});
    } else {
      this.warningBox.hidden = true;
    }
  },

  selectByTimespan() {
    // This method is the onselect handler for the duration dropdown.  As a
    // result it's called a couple of times before onload calls init().
    if (!this._inited) {
      return;
    }

    var warningBox = this.warningBox;

    // If clearing everything
    if (this.selectedTimespan === Sanitizer.TIMESPAN_EVERYTHING) {
      this.prepareWarning();
      if (warningBox.hidden) {
        warningBox.hidden = false;
        let diff =
          warningBox.nextElementSibling.getBoundingClientRect().top -
          warningBox.previousElementSibling.getBoundingClientRect().bottom;
        window.resizeBy(0, diff);
      }
      document.l10n.setAttributes(
        document.documentElement,
        "dialog-title-everything"
      );
      return;
    }

    // If clearing a specific time range
    if (!warningBox.hidden) {
      let diff =
        warningBox.nextElementSibling.getBoundingClientRect().top -
        warningBox.previousElementSibling.getBoundingClientRect().bottom;
      window.resizeBy(0, -diff);
      warningBox.hidden = true;
    }
    document.l10n.setAttributes(document.documentElement, "dialog-title");
  },

  sanitize(event) {
    // Update pref values before handing off to the sanitizer (bug 453440)
    this.updatePrefs();

    // As the sanitize is async, we disable the buttons, update the label on
    // the 'accept' button to indicate things are happening and return false -
    // once the async operation completes (either with or without errors)
    // we close the window.
    let acceptButton = this._dialog.getButton("accept");
    acceptButton.disabled = true;
    document.l10n.setAttributes(acceptButton, "sanitize-button-clearing");
    this._dialog.getButton("cancel").disabled = true;

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
      event.preventDefault();
    } catch (er) {
      Cu.reportError("Exception during sanitize: " + er);
    }
  },

  /**
   * If the panel that displays a warning when the duration is "Everything" is
   * not set up, sets it up.  Otherwise does nothing.
   */
  prepareWarning() {
    // If the date and time-aware locale warning string is ever used again,
    // initialize it here.  Currently we use the no-visits warning string,
    // which does not include date and time.  See bug 480169 comment 48.

    var warningDesc = document.getElementById("sanitizeEverythingWarning");
    if (this.hasNonSelectedItems()) {
      document.l10n.setAttributes(warningDesc, "sanitize-selected-warning");
    } else {
      document.l10n.setAttributes(warningDesc, "sanitize-everything-warning");
    }
  },

  /**
   * Return the boolean prefs that enable/disable clearing of various kinds
   * of history.  The only pref this excludes is privacy.sanitize.timeSpan.
   */
  _getItemPrefs() {
    return Preferences.getAll().filter(
      p => p.id !== "privacy.sanitize.timeSpan"
    );
  },

  /**
   * Called when the value of a preference element is synced from the actual
   * pref.  Enables or disables the OK button appropriately.
   */
  onReadGeneric() {
    // Find any other pref that's checked and enabled (except for
    // privacy.sanitize.timeSpan, which doesn't affect the button's status).
    var found = this._getItemPrefs().some(
      pref => !!pref.value && !pref.disabled
    );

    try {
      this._dialog.getButton("accept").disabled = !found;
    } catch (e) {}

    // Update the warning prompt if needed
    this.prepareWarning();

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
    Preferences.get("privacy.cpd.downloads").value = Preferences.get(
      "privacy.cpd.history"
    ).value;

    // Now manually set the prefs from their corresponding preference
    // elements.
    var prefs = this._getItemPrefs();
    for (let i = 0; i < prefs.length; ++i) {
      var p = prefs[i];
      Services.prefs.setBoolPref(p.id, p.value);
    }
  },

  /**
   * Check if all of the history items have been selected like the default status.
   */
  hasNonSelectedItems() {
    let checkboxes = document.querySelectorAll("checkbox[preference]");
    for (let i = 0; i < checkboxes.length; ++i) {
      let pref = Preferences.get(checkboxes[i].getAttribute("preference"));
      if (!pref.value) {
        return true;
      }
    }
    return false;
  },

  /**
   * Register syncFromPref listener functions.
   */
  registerSyncFromPrefListeners() {
    let checkboxes = document.querySelectorAll("checkbox[preference]");
    for (let checkbox of checkboxes) {
      Preferences.addSyncFromPrefListener(checkbox, () => this.onReadGeneric());
    }
  },

  _titleChanged() {
    let title = document.documentElement.getAttribute("title");
    if (title) {
      document.getElementById("titleText").textContent = title;
    }
  },

  _observeTitleForChanges() {
    this._titleChanged();
    this._mutObs = new MutationObserver(() => {
      this._titleChanged();
    });
    this._mutObs.observe(document.documentElement, {
      attributes: true,
      attributeFilter: ["title"],
    });
  },
};

// We need to give the dialog an opportunity to set up the DOM
// before its measured for the SubDialog it will be embedded in.
// This is because the sanitizeEverythingWarningBox may or may
// not be visible, depending on whether "Everything" is the default
// choice in the menulist.
document.mozSubdialogReady = new Promise(resolve => {
  window.addEventListener(
    "load",
    function() {
      gSanitizePromptDialog.init().then(resolve);
    },
    {
      once: true,
    }
  );
});
