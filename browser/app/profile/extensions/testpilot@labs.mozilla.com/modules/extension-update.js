/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Test Pilot.
 *
 * The Initial Developer of the Original Code is Mozilla.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Raymond Lee <raymond@appcoast.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

