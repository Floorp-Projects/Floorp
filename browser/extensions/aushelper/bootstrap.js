/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

const APP_UPDATE_URL_PREF = "app.update.url";
const REPLACE_KEY = "%OS_VERSION%";

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/TelemetryLog.jsm");

function startup() {
  if (Services.appinfo.OS != "WINNT") {
    return;
  }

  const regCPUPath = "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0";
  let wrk;
  try {
    wrk = Cc["@mozilla.org/windows-registry-key;1"].createInstance(Ci.nsIWindowsRegKey);
    wrk.open(wrk.ROOT_KEY_LOCAL_MACHINE, regCPUPath, wrk.ACCESS_READ);
  } catch (e) {
    Cu.reportError("Unable to open registry. Exception: " + e);
    TelemetryLog.log("AUSHELPER_FATAL_ERROR", [e]);
  }

  // If any of the following values are successfully retrieved and they don't
  // match the condition for that value then it is safe to update. Hence why the
  // following checks are somewhat convoluted. The possible values for the
  // variable set by each check is as follows:
  //
  //          | Match | No Match | Error |
  // variable |  true |   false  |  null |

  let cpuVendorIDMatch = false;
  try {
    let cpuVendorID = wrk.readStringValue("VendorIdentifier");
    if (cpuVendorID.toLowerCase() == "genuineintel") {
      cpuVendorIDMatch = true;
    }
  } catch (e) {
    cpuVendorIDMatch = null;
    Cu.reportError("Error getting CPU vendor indentifier. Exception: " + e);
    TelemetryLog.log("AUSHELPER_CPU_VENDOR_ID_ERROR", [e]);
  }

  let cpuIDMatch = false;
  try {
    let cpuID = wrk.readStringValue("Identifier");
    if (cpuID.toLowerCase().indexOf("family 6 model 61 stepping 4") != -1) {
      cpuIDMatch = true;
    }
  } catch (e) {
    cpuIDMatch = null;
    Cu.reportError("Error getting CPU indentifier. Exception: " + e);
    TelemetryLog.log("AUSHELPER_CPU_ID_ERROR", [e]);
  }

  let microCodeVersions = [0xe, 0x11, 0x12, 0x13, 0x16, 0x18, 0x19];
  let cpuRevMatch = false;
  try {
    let keyNames = ["Update Revision", "Update Signature"];
    for (let i = 0; i < keyNames.length; ++i) {
      try {
        let regVal = wrk.readBinaryValue(keyNames[i]);
        if (regVal.length == 8) {
          let hexVal = [];
          // We are only inyterested in the highest byte and return the little
          // endian value for it.
          for (let j = 4; j < 8; j++) {
            let c = regVal.charCodeAt(j).toString(16);
            if (c.length == 1) {
              c = "0" + c;
            }
            hexVal.unshift(c);
          }
          if (microCodeVersions.indexOf(parseInt(hexVal.join(''))) != -1) {
            cpuRevMatch = true;
          }
          break;
        }
      } catch (e) {
        if (i == keyNames.length - 1) {
          // The registry key name's value was not successfully queried.
          cpuRevMatch = null;
          Cu.reportError("Error getting CPU revision. Exception: " + e);
          TelemetryLog.log("AUSHELPER_CPU_REV_ERROR", [e]);
        }
      }
    }
  } catch (ex) {
    cpuRevMatch = null;
    Cu.reportError("Error getting CPU revision. Exception: " + ex);
    TelemetryLog.log("AUSHELPER_CPU_REV_ERROR", [ex]);
  }

  let resultCode = 3;
  let newValue = "(unkBug1296630v1)";
  // The following uses strict equality checks since the values can be true,
  // false, or null.
  if (cpuVendorIDMatch === false || cpuIDMatch === false || cpuRevMatch === false) {
    // Since one of the values is false then the system won't be affected by
    // bug 1296630 according to the conditions set out in bug 1311515.
    newValue = "(noBug1296630v1)";
    resultCode = 0;
  } else if (cpuVendorIDMatch === null || cpuIDMatch === null || cpuRevMatch === null) {
    // Since one of the values is null we can't say for sure if the system will
    // be affected by bug 1296630.
    newValue = "(errBug1296630v1)";
    resultCode = 2;
  } else if (cpuVendorIDMatch === true && cpuIDMatch === true && cpuRevMatch === true) {
    // Since all of the values are true we can say that the system will be
    // affected by bug 1296630.
    newValue = "(yesBug1296630v1)";
    resultCode = 1;
  }

  let defaultBranch = Services.prefs.getDefaultBranch("");
  let curPrefValue = defaultBranch.getCharPref(APP_UPDATE_URL_PREF);
  let newPrefValue = curPrefValue.replace(REPLACE_KEY + "/", REPLACE_KEY + newValue + "/");
  defaultBranch.setCharPref(APP_UPDATE_URL_PREF, newPrefValue);
  TelemetryLog.log("AUSHELPER_RESULT", [resultCode]);
}

function shutdown() {}
function install() {}
function uninstall() {}
