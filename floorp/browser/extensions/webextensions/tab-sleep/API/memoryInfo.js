/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global ExtensionAPI, ExtensionCommon, Services, XPCOMUtils */

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

this.memoryInfo = class extends ExtensionAPI {
  getAPI(context) {
    return {
      memoryInfo: {
        async getSystemMemorySize() {
          return Services.sysinfo.getProperty("memsize");
        },
      },
    };
  }
};
