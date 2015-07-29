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
                       LIST_NOT_INCLUDE_DUN_INTERFACES) != 0,
          excludeFota: (aConditions &
                        Ci.nsINetworkInterfaceListService.
                        LIST_NOT_INCLUDE_FOTA_INTERFACES) != 0
        }
      )[0]);
  }
};

function FakeNetworkInfo(aAttributes) {
  this.state = aAttributes.state;
  this.type = aAttributes.type;
  this.name = aAttributes.name;
  this.ips = aAttributes.ips;
  this.prefixLengths = aAttributes.prefixLengths;
  this.gateways = aAttributes.gateways;
  this.dnses = aAttributes.dnses;
}
FakeNetworkInfo.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsINetworkInfo]),

  getAddresses: function (ips, prefixLengths) {
    ips.value = this.ips.slice();
    prefixLengths.value = this.prefixLengths.slice();

    return this.ips.length;
  },

  getGateways: function (count) {
    if (count) {
      count.value = this.gateways.length;
    }
    return this.gateways.slice();
  },

  getDnses: function (count) {
    if (count) {
      count.value = this.dnses.length;
    }
    return this.dnses.slice();
  }
};

function NetworkInterfaceList (aInterfaceLiterals) {
  this._interfaces = [];
  for (let entry of aInterfaceLiterals) {
    this._interfaces.push(new FakeNetworkInfo(entry));
  }
}

NetworkInterfaceList.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsINetworkInterfaceList]),

  getNumberOfInterface: function() {
    return this._interfaces.length;
  },

  getInterfaceInfo: function(index) {
    if (!this._interfaces) {
      return null;
    }
    return this._interfaces[index];
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([NetworkInterfaceListService]);

