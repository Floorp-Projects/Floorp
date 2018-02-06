/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const APP_UPDATE_URL_PREF = "app.update.url";
const REPLACE_KEY = "%OS_VERSION%";

const AUSHELPER_CPU_RESULT_CODE_HISTOGRAM_ID = "AUSHELPER_CPU_RESULT_CODE";
// The system is not vulnerable to Bug 1296630.
const CPU_NO_BUG1296630 = 1;
// The system is vulnerable to Bug 1296630.
const CPU_YES_BUG1296630 = 2;
// An error occured when checking if the system is vulnerable to Bug 1296630.
const CPU_ERR_BUG1296630 = 3;
// It is unknown whether the system is vulnerable to Bug 1296630 (should never happen).
const CPU_UNKNOWN_BUG1296630 = 4;

const AUSHELPER_CPU_ERROR_CODE_HISTOGRAM_ID = "AUSHELPER_CPU_ERROR_CODE";
const CPU_SUCCESS = 0;
const CPU_REG_OPEN_ERROR = 1;
const CPU_VENDOR_ID_ERROR = 2;
const CPU_ID_ERROR = 4;
const CPU_REV_ERROR = 8;

const AUSHELPER_WEBSENSE_REG_VERSION_SCALAR_NAME = "aushelper.websense_reg_version";
const AUSHELPER_WEBSENSE_REG_EXISTS_HISTOGRAM_ID = "AUSHELPER_WEBSENSE_REG_EXISTS";

const AUSHELPER_WEBSENSE_ERROR_CODE_HISTOGRAM_ID = "AUSHELPER_WEBSENSE_ERROR_CODE";
const WEBSENSE_SUCCESS = 0;
const WEBSENSE_REG_OPEN_ERROR = 1;
const WEBSENSE_REG_READ_ERROR = 2;
const WEBSENSE_ALREADY_MODIFIED = 4;

ChromeUtils.import("resource://gre/modules/Services.jsm");

function startup() {
  if (Services.appinfo.OS != "WINNT") {
    return;
  }

  const regCPUPath = "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0";
  let wrk;
  let cpuErrorCode = CPU_SUCCESS;
  try {
    wrk = Cc["@mozilla.org/windows-registry-key;1"].createInstance(Ci.nsIWindowsRegKey);
    wrk.open(wrk.ROOT_KEY_LOCAL_MACHINE, regCPUPath, wrk.ACCESS_READ);
  } catch (e) {
    Cu.reportError("AUSHelper - unable to open registry. Exception: " + e);
    cpuErrorCode |= CPU_REG_OPEN_ERROR;
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
    Cu.reportError("AUSHelper - error getting CPU vendor indentifier. Exception: " + e);
    cpuVendorIDMatch = null;
    cpuErrorCode |= CPU_VENDOR_ID_ERROR;
  }

  let cpuIDMatch = false;
  try {
    let cpuID = wrk.readStringValue("Identifier");
    if (cpuID.toLowerCase().includes("family 6 model 61 stepping 4")) {
      cpuIDMatch = true;
    }
  } catch (e) {
    Cu.reportError("AUSHelper - error getting CPU indentifier. Exception: " + e);
    cpuIDMatch = null;
    cpuErrorCode |= CPU_ID_ERROR;
  }

  let microCodeVersions = [0xe, 0x11, 0x12, 0x13, 0x16, 0x18, 0x19];
  let cpuRevMatch = null;
  try {
    let keyNames = ["Update Revision", "Update Signature"];
    for (let i = 0; i < keyNames.length; ++i) {
      try {
        let regVal = wrk.readBinaryValue(keyNames[i]);
        if (regVal.length == 8) {
          let hexVal = [];
          // We are only inyterested in the upper 4 bytes and the little endian
          // value for it.
          for (let j = 4; j < 8; j++) {
            let c = regVal.charCodeAt(j).toString(16);
            if (c.length == 1) {
              c = "0" + c;
            }
            hexVal.unshift(c);
          }
          cpuRevMatch = false;
          if (microCodeVersions.includes(parseInt(hexVal.join("")))) {
            cpuRevMatch = true;
          }
          break;
        }
      } catch (e) {
        if (i == keyNames.length - 1) {
          // The registry key name's value was not successfully queried.
          cpuRevMatch = null;
          cpuErrorCode |= CPU_REV_ERROR;
        }
      }
    }
    wrk.close();
  } catch (ex) {
    Cu.reportError("AUSHelper - error getting CPU revision. Exception: " + ex);
    cpuRevMatch = null;
    cpuErrorCode |= CPU_REV_ERROR;
  }

  let cpuResult = CPU_UNKNOWN_BUG1296630;
  let cpuValue = "(unkBug1296630v1)";
  // The following uses strict equality checks since the values can be true,
  // false, or null.
  if (cpuVendorIDMatch === false || cpuIDMatch === false || cpuRevMatch === false) {
    // Since one of the values is false then the system won't be affected by
    // bug 1296630 according to the conditions set out in bug 1311515.
    cpuValue = "(noBug1296630v1)";
    cpuResult = CPU_NO_BUG1296630;
  } else if (cpuVendorIDMatch === null || cpuIDMatch === null || cpuRevMatch === null) {
    // Since one of the values is null we can't say for sure if the system will
    // be affected by bug 1296630.
    cpuValue = "(errBug1296630v1)";
    cpuResult = CPU_ERR_BUG1296630;
  } else if (cpuVendorIDMatch === true && cpuIDMatch === true && cpuRevMatch === true) {
    // Since all of the values are true we can say that the system will be
    // affected by bug 1296630.
    cpuValue = "(yesBug1296630v1)";
    cpuResult = CPU_YES_BUG1296630;
  }

  Services.telemetry.getHistogramById(AUSHELPER_CPU_RESULT_CODE_HISTOGRAM_ID).add(cpuResult);
  Services.telemetry.getHistogramById(AUSHELPER_CPU_ERROR_CODE_HISTOGRAM_ID).add(cpuErrorCode);

  const regWebsensePath = "Websense\\Agent";
  let websenseErrorCode = WEBSENSE_SUCCESS;
  let websenseVersion = "";
  try {
    let regModes = [wrk.ACCESS_READ, wrk.ACCESS_READ | wrk.WOW64_64];
    for (let i = 0; i < regModes.length; ++i) {
      wrk.open(wrk.ROOT_KEY_LOCAL_MACHINE, "SOFTWARE", regModes[i]);
      try {
        if (wrk.hasChild(regWebsensePath)) {
          let childKey = wrk.openChild(regWebsensePath, wrk.ACCESS_READ);
          websenseVersion = childKey.readStringValue("InstallVersion");
          Services.telemetry.scalarSet(AUSHELPER_WEBSENSE_REG_VERSION_SCALAR_NAME, websenseVersion);
        }
        wrk.close();
      } catch (e) {
        Cu.reportError("AUSHelper - unable to read registry. Exception: " + e);
        websenseErrorCode |= WEBSENSE_REG_READ_ERROR;
      }
    }
  } catch (ex) {
    Cu.reportError("AUSHelper - unable to open registry. Exception: " + ex);
    websenseErrorCode |= WEBSENSE_REG_OPEN_ERROR;
  }

  Services.telemetry.getHistogramById(AUSHELPER_WEBSENSE_REG_EXISTS_HISTOGRAM_ID).add(!!websenseVersion);
  let websenseValue = "(" + (websenseVersion ? "websense-" + websenseVersion : "nowebsense") + ")";

  let branch = Services.prefs.getDefaultBranch("");
  let curValue = branch.getCharPref(APP_UPDATE_URL_PREF);
  if (curValue.indexOf(REPLACE_KEY + "/") > -1) {
    let newValue = curValue.replace(REPLACE_KEY + "/", REPLACE_KEY + cpuValue + websenseValue + "/");
    branch.setCharPref(APP_UPDATE_URL_PREF, newValue);
  } else {
    websenseErrorCode |= WEBSENSE_ALREADY_MODIFIED;
  }
  Services.telemetry.getHistogramById(AUSHELPER_WEBSENSE_ERROR_CODE_HISTOGRAM_ID).add(websenseErrorCode);
}
function shutdown() {}
function install() {}
function uninstall() {}
