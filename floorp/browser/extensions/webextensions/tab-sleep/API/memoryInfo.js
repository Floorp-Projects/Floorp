/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global ExtensionAPI, ExtensionCommon, Services, XPCOMUtils */

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

const gMgr = Cc["@mozilla.org/memory-reporter-manager;1"].getService(
  Ci.nsIMemoryReporterManager
);

this.memoryInfo = class extends ExtensionAPI {
  getAPI(context) {
    return {
      memoryInfo: {
        async getBrowserUsageMemorySize() {
          let total = 0;
          let handleReport = function (
              aProcess,
              aUnsafePath,
              aKind,
              aUnits,
              aAmount,
              aDescription
          ) {
              if (aUnsafePath === "resident-unique") {
                  total += aAmount;
              }
          };
          await new Promise(resolve => {
              gMgr.getReports(
                  handleReport,
                  null,
                  resolve,
                  null,
                  false
              );
          })
          return total;
        },
        async getSystemMemorySize() {
          return Services.sysinfo.getProperty("memsize");
        },
      },
    };
  }
};
