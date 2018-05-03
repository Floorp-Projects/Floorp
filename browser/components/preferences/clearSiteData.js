/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
ChromeUtils.import("resource://gre/modules/DownloadUtils.jsm");
ChromeUtils.import("resource:///modules/SiteDataManager.jsm");

var gClearSiteDataDialog = {
  _clearSiteDataCheckbox: null,
  _clearCacheCheckbox: null,
  _clearButton: null,

  onLoad() {
    document.mozSubdialogReady = this.init();
  },

  async init() {
    this._clearButton = document.getElementById("clearButton");
    this._cancelButton = document.getElementById("cancelButton");
    this._clearSiteDataCheckbox = document.getElementById("clearSiteData");
    this._clearCacheCheckbox = document.getElementById("clearCache");

    // We'll block init() on this because the result values may impact
    // subdialog sizing.
    await Promise.all([
      SiteDataManager.getTotalUsage().then(bytes => {
        let [amount, unit] = DownloadUtils.convertByteUnits(bytes);
        document.l10n.setAttributes(this._clearSiteDataCheckbox,
          "clear-site-data-cookies-with-data", { amount, unit });
      }),
      SiteDataManager.getCacheSize().then(bytes => {
        let [amount, unit] = DownloadUtils.convertByteUnits(bytes);
        document.l10n.setAttributes(this._clearCacheCheckbox,
          "clear-site-data-cache-with-data", { amount, unit });
      }),
    ]);
    await document.l10n.translateElements([
      this._clearCacheCheckbox,
      this._clearSiteDataCheckbox
    ]);

    window.addEventListener("keypress", this.onWindowKeyPress);

    this._cancelButton.addEventListener("command", window.close);
    this._clearButton.addEventListener("command", () => this.onClear());

    this._clearSiteDataCheckbox.addEventListener("command", e => this.onCheckboxCommand(e));
    this._clearCacheCheckbox.addEventListener("command", e => this.onCheckboxCommand(e));
  },

  onWindowKeyPress(event) {
    if (event.keyCode == KeyEvent.DOM_VK_ESCAPE)
      window.close();
  },

  onCheckboxCommand(event) {
    this._clearButton.disabled =
      !(this._clearSiteDataCheckbox.checked || this._clearCacheCheckbox.checked);
  },

  onClear() {
    let allowed = true;

    if (this._clearCacheCheckbox.checked && allowed) {
      SiteDataManager.removeCache();
      // If we're not clearing site data, we need to tell the
      // SiteDataManager to signal that it's updating.
      if (!this._clearSiteDataCheckbox.checked) {
        SiteDataManager.updateSites();
      }
    }

    if (this._clearSiteDataCheckbox.checked) {
      allowed = SiteDataManager.promptSiteDataRemoval(window);
      if (allowed) {
        SiteDataManager.removeSiteData();
      }
    }

    if (allowed) {
      window.close();
    }
  },
};

window.addEventListener("load", () => gClearSiteDataDialog.onLoad());
