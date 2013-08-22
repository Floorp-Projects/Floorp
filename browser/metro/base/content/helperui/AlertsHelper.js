/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var AlertsHelper = {
  _listener: null,
  _cookie: "",

  showAlertNotification: function ah_show(aImageURL, aTitle, aText, aTextClickable, aCookie, aListener) {
    Services.obs.addObserver(this, "metro_native_toast_clicked", false);
    this._listener = aListener;
    this._cookie = aCookie;

    MetroUtils.showNativeToast(aTitle, aText, aImageURL);
  },

  observe: function(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "metro_native_toast_clicked":
        Services.obs.removeObserver(this, "metro_native_toast_clicked");
        this._listener.observe(null, "alertclickcallback", this._cookie);
        break;
    }
  }
};
