/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

Cu.importGlobalProperties(["fetch"]);

var EXPORTED_SYMBOLS = ["TopSiteAttribution"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

var TopSiteAttribution = {
  async makeRequest({ searchProvider, siteURL }) {
    const attributionUrl = Services.prefs.getStringPref(
      `browser.newtabpage.searchTileOverride.${searchProvider}.attributionURL`,
      ""
    );
    if (!attributionUrl) {
      throw new Error(
        `TopSiteAttribution.makeRequest: attribution URL not found for search provider ${searchProvider}`
      );
    }
    const request = new Request(attributionUrl);
    const response = await fetch(request);

    if (response.ok) {
      if (siteURL == response.responseText) {
        console.log("success");
        return;
      }
    }
    console.log("fail");
  },
};
