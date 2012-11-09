/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */

"use strict";

this.EXPORTED_SYMBOLS = ["PhoneNumberUtils"];

const DEBUG = true;
function debug(s) { dump("-*- PhoneNumberutils: " + s + "\n"); }

const Cu = Components.utils;

Cu.import("resource://gre/modules/PhoneNumber.jsm");
Cu.import("resource://gre/modules/mcc_iso3166_table.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "ril",
                                   "@mozilla.org/ril/content-helper;1",
                                   "nsIRILContentHelper");

this.PhoneNumberUtils = {
  //  1. See whether we have a network mcc
  //  2. If we don't have that, look for the simcard mcc
  //  3. TODO: If we don't have that or its 0 (not activated), pick up the last used mcc
  //  4. If we don't have, default to some mcc
  _getCountryName: function() {
    let mcc;
    let countryName;
    // Get network mcc.
    if (ril.voiceConnectionInfo && ril.voiceConnectionInfo.network)
      mcc = ril.voiceConnectionInfo.network.mcc;

    // Get SIM mcc or set it to mcc for Brasil
    if (!mcc)
      mcc = ril.iccInfo.mcc || '724';

    countryName = MCC_ISO3166_TABLE[mcc];
    debug("MCC: " + mcc + "countryName: " + countryName);
    return countryName;
  },

  parse: function(aNumber) {
    let result = PhoneNumber.Parse(aNumber, this._getCountryName());
    debug("InternationalFormat: " + result.internationalFormat);
    debug("InternationalNumber: " + result.internationalNumber);
    debug("NationalNumber: " + result.nationalNumber);
    debug("NationalFormat: " + result.nationalFormat);
    return result;
  },

  parseWithMCC: function(aNumber, aMCC) {
    let countryName = MCC_ISO3166_TABLE[aMCC];
    debug("found country name: " + countryName);
    return PhoneNumber.Parse(aNumber, countryName);
  },
};
