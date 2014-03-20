/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const NETWORKLISTSERVICE_CONTRACTID =
        "@mozilla.org/network/interface-list-service;1";
const NETWORKLISTSERVICE_CID =
        Components.ID("{3780be6e-7012-4e53-ade6-15212fb88a0d}");

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsISyncMessageSender");

function NetworkInterfaceListService () {
}

NetworkInterfaceListService.prototype = {
  classID: NETWORKLISTSERVICE_CID,

  QueryInterface: XPCOMUtils.generateQI([Ci.nsINetworkInterfaceListService]),

  getDataInterfaceList: function(aConditions) {
    return new NetworkInterfaceList(
      cpmm.sendSyncMessage(
        'NetworkInterfaceList:ListInterface',
        {
          excludeSupl: (aConditions &
                        Ci.nsINetworkInterfaceListService.
                        LIST_NOT_INCLUDE_SUPL_INTERFACES) != 0,
          excludeMms: (aConditions &
                       Ci.nsINetworkInterfaceListService.
                       LIST_NOT_INCLUDE_MMS_INTERFACES) != 0,
          excludeIms: (aConditions &
                       Ci.nsINetworkInterfaceListService.
                       LIST_NOT_INCLUDE_IMS_INTERFACES) != 0,
          excludeDun: (aConditions &
                       Ci.nsINetworkInterfaceListService.
                       LIST_NOT_INCLUDE_DUN_INTERFACES) != 0
        }
      )[0]);
  }
};

function NetworkInterfaceList (aInterfaces) {
  this._interfaces = aInterfaces;
}

NetworkInterfaceList.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsINetworkInterfaceList]),
  getNumberOfInterface: function() {
    return this._interfaces.length;
  },

  getInterface: function(index) {
    if (!this._interfaces) {
      return null;
    }
    return this._interfaces[index];
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([NetworkInterfaceListService]);

