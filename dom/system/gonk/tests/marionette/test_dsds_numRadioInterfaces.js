/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_CONTEXT = "chrome";

Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/systemlibs.js");

const NS_RIL_CONTRACTID = "@mozilla.org/ril;1";

const PROP_RO_MOZ_RIL_NUMCLIENTS = "ro.moz.ril.numclients";

const PREF_RIL_NUM_RADIO_INTERFACES = "ril.numRadioInterfaces";

ok(libcutils, "libcutils is available");

let propNum = (function() {
  try {
    let numString = libcutils.property_get(PROP_RO_MOZ_RIL_NUMCLIENTS, "1");
    let num = parseInt(numString, 10);
    if (num >= 0) {
      return num;
    }
  } catch (e) {}
})();

log("Retrieved '" + PROP_RO_MOZ_RIL_NUMCLIENTS + "' = " + propNum);
ok(propNum, PROP_RO_MOZ_RIL_NUMCLIENTS);

let prefNum = Services.prefs.getIntPref(PREF_RIL_NUM_RADIO_INTERFACES);
log("Retrieved '" + PREF_RIL_NUM_RADIO_INTERFACES + "' = " + prefNum);

let ril = Cc[NS_RIL_CONTRACTID].getService(Ci.nsIRadioInterfaceLayer);
ok(ril, "ril.constructor is " + ril.constructor);

let ifaceNum = ril.numRadioInterfaces;
log("Retrieved 'nsIRadioInterfaceLayer.numRadioInterfaces' = " + ifaceNum);

is(propNum, prefNum);
is(propNum, ifaceNum);

finish();
