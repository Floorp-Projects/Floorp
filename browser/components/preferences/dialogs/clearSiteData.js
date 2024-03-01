/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { SiteDataManager } = ChromeUtils.importESModule(
  "resource:///modules/SiteDataManager.sys.mjs"
);

ChromeUtils.defineESModuleGetters(this, {
  DownloadUtils: "resource://gre/modules/DownloadUtils.sys.mjs",
});

var gClearSiteDataDialog = {
  _clearSiteDataCheckbox: null,
  _clearCacheCheckbox: null,

  onLoad() {
    document.mozSubdialogReady = this.init();
  },

  async init() {
    this._dialog = document.querySelector("dialog");
    this._clearSiteDataCheckbox = document.getElementById("clearSiteData");
    this._clearCacheCheckbox = document.getElementById("clearCache");

    // We'll block init() on this because the result values may impact
    // subdialog sizing.
    await Promise.all([
      SiteDataManager.getTotalUsage().then(bytes => {
        let [amount, unit] = DownloadUtils.convertByteUnits(bytes);
        document.l10n.setAttributes(
          this._clearSiteDataCheckbox,
          "clear-site-data-cookies-with-data",
          { amount, unit }
        );
      }),
      SiteDataManager.getCacheSize().then(bytes => {
        let [amount, unit] = DownloadUtils.convertByteUnits(bytes);
        document.l10n.setAttributes(
          this._clearCacheCheckbox,
          "clear-site-data-cache-with-data",
          { amount, unit }
        );
      }),
    ]);
    await document.l10n.translateElements([
      this._clearCacheCheckbox,
      this._clearSiteDataCheckbox,
    ]);

    document.addEventListener("dialogaccept", event => this.onClear(event));

    this._clearSiteDataCheckbox.addEventListener("command", e =>
      this.onCheckboxCommand(e)
    );
    this._clearCacheCheckbox.addEventListener("command", e =>
      this.onCheckboxCommand(e)
    );
  },

  onCheckboxCommand() {
    this._dialog.setAttribute(
      "buttondisabledaccept",
      !(this._clearSiteDataCheckbox.checked || this._clearCacheCheckbox.checked)
    );
  },

  onClear(event) {
    let clearSiteData = this._clearSiteDataCheckbox.checked;
    let clearCache = this._clearCacheCheckbox.checked;

    if (clearSiteData) {
      // Ask for confirmation before clearing site data
      if (!SiteDataManager.promptSiteDataRemoval(window)) {
        clearSiteData = false;
        // Prevent closing the dialog when the data removal wasn't allowed.
        event.preventDefault();
      }
    }

    if (clearSiteData) {
      SiteDataManager.removeSiteData();
    }
    if (clearCache) {
      SiteDataManager.removeCache();

      // If we're not clearing site data, we need to tell the
      // SiteDataManager to signal that it's updating.
      if (!clearSiteData) {
        SiteDataManager.updateSites();
      }
    }
  },
};

window.addEventListener("load", () => gClearSiteDataDialog.onLoad());
