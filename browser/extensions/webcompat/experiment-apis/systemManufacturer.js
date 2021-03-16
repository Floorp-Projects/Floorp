/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global ExtensionAPI, Services, XPCOMUtils */

this.systemManufacturer = class extends ExtensionAPI {
  getAPI(context) {
    return {
      systemManufacturer: {
        getManufacturer() {
          try {
            return Services.sysinfo.getProperty("manufacturer");
          } catch (_) {
            return undefined;
          }
        },
      },
    };
  }
};
