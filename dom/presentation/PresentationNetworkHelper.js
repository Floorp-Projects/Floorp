// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Messaging.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const NETWORKHELPER_CID = Components.ID("{5fb96caa-6d49-4f6b-9a4b-65dd0d51f92d}");

function PresentationNetworkHelper() {}

PresentationNetworkHelper.prototype = {
  classID: NETWORKHELPER_CID,
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPresentationNetworkHelper]),

  getWifiIPAddress: function(aListener) {
    Messaging.sendRequestForResult({type: "Wifi:GetIPAddress"})
             .then(result => aListener.onGetWifiIPAddress(result),
                   err => aListener.onError(err));
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([PresentationNetworkHelper]);
