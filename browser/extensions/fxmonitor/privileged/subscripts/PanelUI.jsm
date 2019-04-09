/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals XUL_NS, Services, PluralForm */

function PanelUI(doc) {
  this.site = null;
  this.doc = doc;

  let box = doc.createElementNS(XUL_NS, "vbox");

  let elt = doc.createElementNS(XUL_NS, "description");
  elt.textContent = this.getString("fxmonitor.popupHeader");
  elt.classList.add("headerText");
  box.appendChild(elt);

  elt = doc.createElementNS(XUL_NS, "description");
  elt.classList.add("popupText");
  box.appendChild(elt);

  this.box = box;
}

PanelUI.prototype = {
  get FirefoxMonitorUtils() {
    // Set on every window by FirefoxMonitor.jsm for PanelUI to use.
    // Because sharing is caring.
    return this.doc.defaultView.FirefoxMonitorUtils;
  },

  getString(aKey) {
    return this.FirefoxMonitorUtils.getString(aKey);
  },

  getFormattedString(aKey, args) {
    return this.FirefoxMonitorUtils.getFormattedString(aKey, args);
  },

  get brandString() {
    if (this._brandString) {
      return this._brandString;
    }
    return this._brandString = this.getString("fxmonitor.brandName");
  },

  get primaryAction() {
    if (this._primaryAction) {
      return this._primaryAction;
    }
    return this._primaryAction = {
      label: this.getFormattedString("fxmonitor.checkButton.label", [this.brandString]),
      accessKey: this.getString("fxmonitor.checkButton.accessKey"),
      callback: () => {
        let win = this.doc.defaultView;
        win.openTrustedLinkIn(
          win.FirefoxMonitorUtils.getFirefoxMonitorURL(this.site.Name), "tab", { });

        Services.telemetry.recordEvent("fxmonitor", "interaction", "check_btn");
      },
    };
  },

  get secondaryActions() {
    if (this._secondaryActions) {
      return this._secondaryActions;
    }
    return this._secondaryActions = [
      {
        label: this.getString("fxmonitor.dismissButton.label"),
        accessKey: this.getString("fxmonitor.dismissButton.accessKey"),
        callback: () => {
          Services.telemetry.recordEvent("fxmonitor", "interaction", "dismiss_btn");
        },
      }, {
        label: this.getFormattedString("fxmonitor.neverShowButton.label", [this.brandString]),
        accessKey: this.getString("fxmonitor.neverShowButton.accessKey"),
        callback: () => {
          this.FirefoxMonitorUtils.disable();
          Services.telemetry.recordEvent("fxmonitor", "interaction", "never_show_btn");
        },
      },
    ];
  },

  refresh(site) {
    this.site = site;

    let elt = this.box.querySelector(".popupText");

    // If > 100k, the PwnCount is rounded down to the most significant
    // digit and prefixed with "More than".
    // Ex.: 12,345 -> 12,345
    //      234,567 -> More than 200,000
    //      345,678,901 -> More than 300,000,000
    //      4,567,890,123 -> More than 4,000,000,000
    let k100k = 100000;
    let pwnCount = site.PwnCount;
    let stringName = "fxmonitor.popupText";
    if (pwnCount > k100k) {
      let multiplier = 1;
      while (pwnCount >= 10) {
        pwnCount /= 10;
        multiplier *= 10;
      }
      pwnCount = Math.floor(pwnCount) * multiplier;
      stringName = "fxmonitor.popupTextRounded";
    }

    elt.textContent =
      PluralForm.get(pwnCount, this.getString(stringName))
                .replace("#1", pwnCount.toLocaleString())
                .replace("#2", site.Name)
                .replace("#3", site.Year)
                .replace("#4", this.brandString);
  },
};
