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
 * The Original Code is Firefox Sync.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Philipp von Weitershausen <philipp@weitershausen.de>
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

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;

Cu.import("resource://services-sync/main.js");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const PIN_PART_LENGTH = 4;

const ADD_DEVICE_PAGE       = 0;
const SYNC_KEY_PAGE         = 1;
const DEVICE_CONNECTED_PAGE = 2;

let gSyncAddDevice = {

  init: function init() {
    this.pin1.setAttribute("maxlength", PIN_PART_LENGTH);
    this.pin2.setAttribute("maxlength", PIN_PART_LENGTH);
    this.pin3.setAttribute("maxlength", PIN_PART_LENGTH);

    this.nextFocusEl = {pin1: this.pin2,
                        pin2: this.pin3,
                        pin3: this.wizard.getButton("next")};

    this.throbber = document.getElementById("pairDeviceThrobber");
    this.errorRow = document.getElementById("errorRow");

    // Kick off a sync. That way the server will have the most recent data from
    // this computer and it will show up immediately on the new device.
    Weave.Utils.nextTick(Weave.Service.sync, Weave.Service);
  },

  onPageShow: function onPageShow() {
    this.wizard.getButton("back").hidden = true;

    switch (this.wizard.pageIndex) {
      case ADD_DEVICE_PAGE:
        this.onTextBoxInput();
        this.wizard.canRewind = false;
        this.wizard.getButton("next").hidden = false;
        this.pin1.focus();
        break;
      case SYNC_KEY_PAGE:
        this.wizard.canAdvance = false;
        this.wizard.canRewind = true;
        this.wizard.getButton("back").hidden = false;
        this.wizard.getButton("next").hidden = true;
        document.getElementById("weavePassphrase").value =
          Weave.Utils.hyphenatePassphrase(Weave.Service.passphrase);
        break;
      case DEVICE_CONNECTED_PAGE:
        this.wizard.canAdvance = true;
        this.wizard.canRewind = false;
        this.wizard.getButton("cancel").hidden = true;
        break;
    }
  },

  onWizardAdvance: function onWizardAdvance() {
    switch (this.wizard.pageIndex) {
      case ADD_DEVICE_PAGE:
        this.startTransfer();
        return false;
      case DEVICE_CONNECTED_PAGE:
        window.close();
        return false;
    }
    return true;
  },

  startTransfer: function startTransfer() {
    this.errorRow.hidden = true;
    // When onAbort is called, Weave may already be gone.
    const JPAKE_ERROR_USERABORT = Weave.JPAKE_ERROR_USERABORT;

    let self = this;
    let jpakeclient = this._jpakeclient = new Weave.JPAKEClient({
      onPaired: function onPaired() {
        let credentials = {account:   Weave.Service.account,
                           password:  Weave.Service.password,
                           synckey:   Weave.Service.passphrase,
                           serverURL: Weave.Service.serverURL};
        jpakeclient.sendAndComplete(credentials);
      },
      onComplete: function onComplete() {
        delete self._jpakeclient;
        self.wizard.pageIndex = DEVICE_CONNECTED_PAGE;

        // Schedule a Sync for soonish to fetch the data uploaded by the
        // device with which we just paired.
        Weave.SyncScheduler.scheduleNextSync(Weave.SyncScheduler.activeInterval);
      },
      onAbort: function onAbort(error) {
        delete self._jpakeclient;

        // Aborted by user, ignore.
        if (error == JPAKE_ERROR_USERABORT) {
          return;
        }

        self.errorRow.hidden = false;
        self.throbber.hidden = true;
        self.pin1.value = self.pin2.value = self.pin3.value = "";
        self.pin1.disabled = self.pin2.disabled = self.pin3.disabled = false;
        self.pin1.focus();
      }
    });
    this.throbber.hidden = false;
    this.pin1.disabled = this.pin2.disabled = this.pin3.disabled = true;
    this.wizard.canAdvance = false;

    let pin = this.pin1.value + this.pin2.value + this.pin3.value;
    let expectDelay = false;
    jpakeclient.pairWithPIN(pin, expectDelay);
  },

  onWizardBack: function onWizardBack() {
    if (this.wizard.pageIndex != SYNC_KEY_PAGE)
      return true;

    this.wizard.pageIndex = ADD_DEVICE_PAGE;
    return false;
  },

  onWizardCancel: function onWizardCancel() {
    if (this._jpakeclient) {
      this._jpakeclient.abort();
      delete this._jpakeclient;
    }
    return true;
  },

  onTextBoxInput: function onTextBoxInput(textbox) {
    if (textbox && textbox.value.length == PIN_PART_LENGTH)
      this.nextFocusEl[textbox.id].focus();

    this.wizard.canAdvance = (this.pin1.value.length == PIN_PART_LENGTH
                              && this.pin2.value.length == PIN_PART_LENGTH
                              && this.pin3.value.length == PIN_PART_LENGTH);
  },

  goToSyncKeyPage: function goToSyncKeyPage() {
    this.wizard.pageIndex = SYNC_KEY_PAGE;
  }

};
// onWizardAdvance() and onPageShow() are run before init() so we'll set
// these up as lazy getters.
["wizard", "pin1", "pin2", "pin3"].forEach(function (id) {
  XPCOMUtils.defineLazyGetter(gSyncAddDevice, id, function() {
    return document.getElementById(id);
  });
});
