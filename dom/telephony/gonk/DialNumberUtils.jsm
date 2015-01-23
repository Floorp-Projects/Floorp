/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["DialNumberUtils"];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/systemlibs.js");

const DEFAULT_EMERGENCY_NUMBERS = ["112", "911"];

const MMI_MATCH_GROUP_FULL_MMI = 1;
const MMI_MATCH_GROUP_PROCEDURE = 2;
const MMI_MATCH_GROUP_SERVICE_CODE = 3;
const MMI_MATCH_GROUP_SIA = 4;
const MMI_MATCH_GROUP_SIB = 5;
const MMI_MATCH_GROUP_SIC = 6;
const MMI_MATCH_GROUP_PWD_CONFIRM = 7;
const MMI_MATCH_GROUP_DIALING_NUMBER = 8;

this.DialNumberUtils = {
  /**
   * Check a given number against the list of emergency numbers provided by the
   * RIL.
   */
  isEmergency: function(aNumber) {
    // Check ril provided numbers first.
    let numbers = libcutils.property_get("ril.ecclist") ||
                  libcutils.property_get("ro.ril.ecclist");
    if (numbers) {
      numbers = numbers.split(",");
    } else {
      // No ecclist system property, so use our own list.
      numbers = DEFAULT_EMERGENCY_NUMBERS;
    }
    return numbers.indexOf(aNumber) != -1;
  },

  _mmiRegExp: (function() {
    // Procedure, which could be *, #, *#, **, ##
    let procedure = "(\\*[*#]?|##?)";

    // Service code, which is a 2 or 3 digits that uniquely specifies the
    // Supplementary Service associated with the MMI code.
    let serviceCode = "(\\d{2,3})";

    // Supplementary Information SIA, SIB and SIC.
    // Where a particular service request does not require any SI,  "*SI" is
    // not entered. The use of SIA, SIB and SIC is optional and shall be
    // entered in any of the following formats:
    //    - *SIA*SIB*SIC#
    //    - *SIA*SIB#
    //    - *SIA**SIC#
    //    - *SIA#
    //    - **SIB*SIC#
    //    - ***SIC#
    //
    // Also catch the additional NEW_PASSWORD for the case of a password
    // registration procedure. Ex:
    //    - *  03 * ZZ * OLD_PASSWORD * NEW_PASSWORD * NEW_PASSWORD #
    //    - ** 03 * ZZ * OLD_PASSWORD * NEW_PASSWORD * NEW_PASSWORD #
    //    - *  03 **     OLD_PASSWORD * NEW_PASSWORD * NEW_PASSWORD #
    //    - ** 03 **     OLD_PASSWORD * NEW_PASSWORD * NEW_PASSWORD #
    let si = "\\*([^*#]*)";
    let allSi = "";
    for (let i = 0; i < 4; ++i) {
      allSi = "(?:" + si + allSi + ")?";
    }

    let fullmmi = "(" + procedure + serviceCode + allSi + "#)";

    // Dial string after the #.
    let optionalDialString = "([^#]+)?";

    return new RegExp("^" + fullmmi + optionalDialString + "$");
  })(),

  _isPoundString: function(aString) {
    return aString && aString[aString.length - 1] === "#";
  },

  _isShortString: function(aString) {
    if (!aString || this.isEmergency(aString) || aString.length > 2 ||
        (aString.length == 2 && aString[0] === "1")) {
      return false;
    }
    return true;
  },

  /**
   * Check parse the given string as an MMI code.
   *
   * An MMI code should be:
   * - Activation (*SC*SI#).
   * - Deactivation (#SC*SI#).
   * - Interrogation (*#SC*SI#).
   * - Registration (**SC*SI#).
   * - Erasure (##SC*SI#).
   * where SC = Service Code (2 or 3 digits) and SI = Supplementary Info
   * (variable length).
   */
  parseMMI: function(aString) {
    let matches = this._mmiRegExp.exec(aString);
    if (matches) {
      return {
        fullMMI: matches[MMI_MATCH_GROUP_FULL_MMI],
        procedure: matches[MMI_MATCH_GROUP_PROCEDURE],
        serviceCode: matches[MMI_MATCH_GROUP_SERVICE_CODE],
        sia: matches[MMI_MATCH_GROUP_SIA],
        sib: matches[MMI_MATCH_GROUP_SIB],
        sic: matches[MMI_MATCH_GROUP_SIC],
        pwd: matches[MMI_MATCH_GROUP_PWD_CONFIRM],
        dialNumber: matches[MMI_MATCH_GROUP_DIALING_NUMBER]
      };
    }

    if (this._isPoundString(aString) || this._isShortString(aString)) {
      return {
        fullMMI: aString
      };
    }

    return null;
  }
};
