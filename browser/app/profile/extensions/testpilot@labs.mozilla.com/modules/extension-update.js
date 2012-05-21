/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

EXPORTED_SYMBOLS = ["TestPilotExtensionUpdate"];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

let TestPilotExtensionUpdate = {
  check : function(extensionId) {
    let extensionManager =
      Cc["@mozilla.org/extensions/manager;1"].
        getService(Ci.nsIExtensionManager);
    let listener = new TestPilotExtensionUpdateCheckListener();
    let items = [extensionManager.getItemForID(extensionId)];
    extensionManager.update(
      items, items.length, false, listener,
      Ci.nsIExtensionManager.UPDATE_WHEN_USER_REQUESTED);
  }
};

function TestPilotExtensionUpdateCheckListener() {
}

TestPilotExtensionUpdateCheckListener.prototype = {
  _addons: [],

  onUpdateStarted: function() {
  },

  onUpdateEnded: function() {
    if (this._addons.length > 0) {
      let extensionManager =
        Cc["@mozilla.org/extensions/manager;1"].
          getService(Ci.nsIExtensionManager);
      let wm =
        Cc["@mozilla.org/appshell/window-mediator;1"].
          getService(Ci.nsIWindowMediator);
      let win = wm.getMostRecentWindow("navigator:browser");

      extensionManager.addDownloads(this._addons, this._addons.length, null);
      win.BrowserOpenAddonsMgr("extensions");
    }
  },

  onAddonUpdateStarted: function(addon) {
  },

  onAddonUpdateEnded: function(addon, status) {
    if (status == Ci.nsIAddonUpdateCheckListener.STATUS_UPDATE) {
      this._addons.push(addon);
    }
  }
};

