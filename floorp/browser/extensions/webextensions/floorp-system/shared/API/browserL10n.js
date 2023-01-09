/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global ExtensionAPI, ExtensionCommon, Services, XPCOMUtils */


const jsmScope = ChromeUtils.import(
  "resource://devtools/shared/loader/Loader.jsm"
);

const { Localization } = Cu.getGlobalForObject(jsmScope);

this.browserL10n = class extends ExtensionAPI {
  getAPI(context) {
    return {
      browserL10n: {
        async getFloorpL10nValues(id) {
          let floorpLocalization = await new Localization(id.file)
          let value = await floorpLocalization.formatValues(id.text);
          return value;
        },
      },
    };
  }
};
