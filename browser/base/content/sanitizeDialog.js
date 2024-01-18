/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from /toolkit/content/preferencesBindings.js */

var { Sanitizer } = ChromeUtils.importESModule(
  "resource:///modules/Sanitizer.sys.mjs"
);

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  DownloadUtils: "resource://gre/modules/DownloadUtils.sys.mjs",
  SiteDataManager: "resource:///modules/SiteDataManager.sys.mjs",
});

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "USE_OLD_DIALOG",
  "privacy.sanitize.useOldClearHistoryDialog",
  false
);

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
  { id: "privacy.clearOnShutdown.history", type: "bool" },
  {
    id: "privacy.clearOnShutdown_v2.historyFormDataAndDownloads",
    type: "bool",
  },
  { id: "privacy.clearOnShutdown.formdata", type: "bool" },
  { id: "privacy.clearOnShutdown.downloads", type: "bool" },
  { id: "privacy.clearOnShutdown_v2.downloads", type: "bool" },
  { id: "privacy.clearOnShutdown.cookies", type: "bool" },
  { id: "privacy.clearOnShutdown_v2.cookiesAndStorage", type: "bool" },
  { id: "privacy.clearOnShutdown.cache", type: "bool" },
  { id: "privacy.clearOnShutdown_v2.cache", type: "bool" },
  { id: "privacy.clearOnShutdown.offlineApps", type: "bool" },
  { id: "privacy.clearOnShutdown.sessions", type: "bool" },
  { id: "privacy.clearOnShutdown.siteSettings", type: "bool" },
  { id: "privacy.clearOnShutdown_v2.siteSettings", type: "bool" },
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
    /**
     * Variables to store data sizes to display to user
     * for different timespans
     */
    this.siteDataSizes = {};
    this.cacheSize = [];

    if (!lazy.USE_OLD_DIALOG) {
      this._cookiesAndSiteDataCheckbox =
        document.getElementById("cookiesAndStorage");
      this._cacheCheckbox = document.getElementById("cache");
    }

    let arg = window.arguments?.[0] || {};

    // The updateUsageData variable allows callers of the dialog to indicate
    // whether site usage data should be refreshed on init.
    let updateUsageData = true;
    if (!lazy.USE_OLD_DIALOG && arg.updateUsageData != undefined) {
      updateUsageData = arg.updateUsageData || arg.inBrowserWindow;
    }

    // These variables decide which context the dialog has been opened in
    this._inClearOnShutdownNewDialog = false;
    this._inClearSiteDataNewDialog = false;
    if (arg.mode && !lazy.USE_OLD_DIALOG) {
      this._inClearOnShutdownNewDialog = arg.mode == "clearOnShutdown";
      this._inClearSiteDataNewDialog = arg.mode == "clearSiteData";
    }

    // Clear site data has it's own default checked boxes, all other entry points
    // follow the clear history default prefs
    this.defaultCheckedByContext = {
      clearHistory: [
        "historyFormDataAndDownloads",
        "cookiesAndStorage",
        "cache",
      ],
      clearSiteData: ["cookiesAndStorage", "cache"],
    };

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

    this.dataSizesFinishedUpdatingPromise =
      this.getAndUpdateDataSizes(updateUsageData);

    let OKButton = this._dialog.getButton("accept");
    let clearOnShutdownGroupbox = document.getElementById(
      "clearOnShutdownGroupbox"
    );
    let clearPrivateDataGroupbox = document.getElementById(
      "clearPrivateDataGroupbox"
    );

    let okButtonl10nID = "sanitize-button-ok";
    if (this._inClearOnShutdownNewDialog) {
      okButtonl10nID = "sanitize-button-ok-on-shutdown";
      this._dialog.setAttribute("inClearOnShutdown", "true");
      // remove the clear private data groupbox element
      clearPrivateDataGroupbox.remove();
    } else if (!lazy.USE_OLD_DIALOG) {
      okButtonl10nID = "sanitize-button-ok2";
      // remove the clear on shutdown groupbox element
      clearOnShutdownGroupbox.remove();
    }
    document.l10n.setAttributes(OKButton, okButtonl10nID);

    // update initial checkbox values based on the context the dialog is opened
    // from (history, site data). Categories are not remembered
    // from the last time the dialog was used.
    if (!lazy.USE_OLD_DIALOG && !this._inClearOnShutdownNewDialog) {
      let checkboxes = document.querySelectorAll(
        "#clearPrivateDataGroupbox .clearingItemCheckbox"
      );
      let defaults = this.defaultCheckedByContext.clearHistory;
      if (this._inClearSiteDataNewDialog) {
        defaults = this.defaultCheckedByContext.clearSiteData;
      }

      for (let checkbox of checkboxes) {
        let pref = checkbox.id;
        let value = false;
        if (defaults.includes(pref)) {
          value = true;
          checkbox.checked = value;
        }
      }
    }

    document.addEventListener("dialogaccept", e => {
      if (this._inClearOnShutdownNewDialog) {
        this.updatePrefs();
      } else {
        this.sanitize(e);
      }
    });

    this.registerSyncFromPrefListeners();

    // we want to show the warning box for all cases except clear on shutdown
    if (
      this.selectedTimespan === Sanitizer.TIMESPAN_EVERYTHING &&
      !arg.inClearOnShutdown
    ) {
      this.prepareWarning();
      this.warningBox.hidden = false;
      if (lazy.USE_OLD_DIALOG) {
        document.l10n.setAttributes(
          document.documentElement,
          "sanitize-dialog-title-everything"
        );
      }
      let warningDesc = document.getElementById("sanitizeEverythingWarning");
      // Ensure we've translated and sized the warning.
      await document.l10n.translateFragment(warningDesc);
      let rootWin = window.browsingContext.topChromeWindow;
      await rootWin.promiseDocumentFlushed(() => {});
    } else {
      this.warningBox.hidden = true;
    }

    await this.dataSizesFinishedUpdatingPromise;
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

      // update title for the old dialog
      if (lazy.USE_OLD_DIALOG) {
        document.l10n.setAttributes(
          document.documentElement,
          "sanitize-dialog-title-everything"
        );
      }
      // make sure the sizes are updated in the new dialog
      else {
        this.updateDataSizesInUI();
      }
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
    let datal1OnId = lazy.USE_OLD_DIALOG
      ? "sanitize-dialog-title"
      : "sanitize-dialog-title2";
    document.l10n.setAttributes(document.documentElement, datal1OnId);

    if (!lazy.USE_OLD_DIALOG) {
      // We only update data sizes to display on the new dialog
      this.updateDataSizesInUI();
    }
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
      let itemsToClear = this.getItemsToClear();
      Sanitizer.sanitize(itemsToClear, options)
        .catch(console.error)
        .then(() => window.close())
        .catch(console.error);
      event.preventDefault();
    } catch (er) {
      console.error("Exception during sanitize: ", er);
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
   * Return the boolean prefs that correspond to the checkboxes on the dialog.
   */
  _getItemPrefs() {
    return Preferences.getAll().filter(pref => {
      // The timespan pref isn't a bool, so don't return it
      if (pref.id == "privacy.sanitize.timeSpan") {
        return false;
      }
      // In the old dialog, cpd.downloads isn't controlled by a checkbox
      return !(lazy.USE_OLD_DIALOG && pref.id == "privacy.cpd.downloads");
    });
  },

  /**
   * Called when the value of a preference element is synced from the actual
   * pref.  Enables or disables the OK button appropriately.
   */
  onReadGeneric() {
    // Find any other pref that's checked and enabled (except for
    // privacy.sanitize.timeSpan, which doesn't affect the button's status.
    // and (in the old dialog) privacy.cpd.downloads which is not controlled
    // directly by a checkbox).
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
   * Gets the latest usage data and then updates the UI
   *
   * @param {boolean} doUpdateSites - if we need to trigger an
   *        updateSites() to get the latest usage data
   * @returns {Promise} resolves when updating the UI is complete
   */
  async getAndUpdateDataSizes(doUpdateSites) {
    if (lazy.USE_OLD_DIALOG) {
      return;
    }
    if (doUpdateSites) {
      await lazy.SiteDataManager.updateSites();
    }
    // Current timespans used in the dialog box
    const ALL_TIMESPANS = [
      "TIMESPAN_HOUR",
      "TIMESPAN_2HOURS",
      "TIMESPAN_4HOURS",
      "TIMESPAN_TODAY",
      "TIMESPAN_EVERYTHING",
    ];

    let [quotaUsage, cacheSize] = await Promise.all([
      lazy.SiteDataManager.getQuotaUsageForTimeRanges(ALL_TIMESPANS),
      lazy.SiteDataManager.getCacheSize(),
    ]);
    // Convert sizes to [amount, unit]
    for (const timespan in quotaUsage) {
      this.siteDataSizes[timespan] = lazy.DownloadUtils.convertByteUnits(
        quotaUsage[timespan]
      );
    }
    this.cacheSize = lazy.DownloadUtils.convertByteUnits(cacheSize);
    this.updateDataSizesInUI();
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

    if (lazy.USE_OLD_DIALOG) {
      let historyValue = Preferences.get(`privacy.cpd.history`).value;
      // Keep the pref for the download history in sync with the history pref.
      Preferences.get("privacy.cpd.downloads").value = historyValue;
      Services.prefs.setBoolPref("privacy.cpd.downloads", historyValue);
    }

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

  /**
   * Updates data sizes displayed based on new selected timespan
   */
  updateDataSizesInUI() {
    const TIMESPAN_SELECTION_MAP = {
      0: "TIMESPAN_EVERYTHING",
      1: "TIMESPAN_HOUR",
      2: "TIMESPAN_2HOURS",
      3: "TIMESPAN_4HOURS",
      4: "TIMESPAN_TODAY",
      5: "TIMESPAN_5MINS",
      6: "TIMESPAN_24HOURS",
    };
    let index = this.selectedTimespan;
    let timeSpanSelected = TIMESPAN_SELECTION_MAP[index];
    let [amount, unit] = this.siteDataSizes[timeSpanSelected];

    document.l10n.setAttributes(
      this._cookiesAndSiteDataCheckbox,
      "item-cookies-site-data-with-size",
      { amount, unit }
    );

    [amount, unit] = this.cacheSize;
    document.l10n.setAttributes(
      this._cacheCheckbox,
      "item-cached-content-with-size",
      { amount, unit }
    );
  },

  /**
   * Get all items to clear based on checked boxes
   *
   * @returns {string[]} array of items ["cache", "historyFormDataAndDownloads"...]
   */
  getItemsToClear() {
    // the old dialog uses the preferences to decide what to clear
    if (lazy.USE_OLD_DIALOG) {
      return null;
    }

    let items = [];
    let clearPrivateDataGroupbox = document.getElementById(
      "clearPrivateDataGroupbox"
    );

    for (let cb of clearPrivateDataGroupbox.querySelectorAll("checkbox")) {
      if (cb.checked) {
        items.push(cb.id);
      }
    }
    return items;
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
    function () {
      gSanitizePromptDialog.init().then(resolve);
    },
    {
      once: true,
    }
  );
});
