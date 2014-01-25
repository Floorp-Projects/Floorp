# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

XPCOMUtils.defineLazyGetter(this, "FxAccountsCommon", function () {
  return Cu.import("resource://gre/modules/FxAccountsCommon.js", {});
});

let gFxAccounts = {

  get topics() {
    delete this.topics;
    return this.topics = [
      FxAccountsCommon.ONVERIFIED_NOTIFICATION
    ];
  },

  init: function () {
    for (let topic of this.topics) {
      Services.obs.addObserver(this, topic, false);
    }
  },

  uninit: function () {
    for (let topic of this.topics) {
      Services.obs.removeObserver(this, topic);
    }
  },

  observe: function (subject, topic) {
    this.showDoorhanger();
  },

  showDoorhanger: function () {
    let panel = document.getElementById("sync-popup");
    let anchor = document.getElementById("PanelUI-menu-button");

    let iconAnchor =
      document.getAnonymousElementByAttribute(anchor, "class",
                                              "toolbarbutton-icon");

    panel.hidden = false;
    panel.openPopup(iconAnchor || anchor, "bottomcenter topright");
  }
};
