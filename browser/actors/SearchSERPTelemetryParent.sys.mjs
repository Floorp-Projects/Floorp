/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  SearchSERPTelemetry: "resource:///modules/SearchSERPTelemetry.sys.mjs",
});

export class SearchSERPTelemetryParent extends JSWindowActorParent {
  receiveMessage(msg) {
    let browser = this.browsingContext.top.embedderElement;

    switch (msg.name) {
      case "SearchTelemetry:PageInfo": {
        lazy.SearchSERPTelemetry.reportPageWithAds(msg.data, browser);
        break;
      }
      case "SearchTelemetry:AdImpressions": {
        lazy.SearchSERPTelemetry.reportPageWithAdImpressions(msg.data, browser);
        break;
      }
      case "SearchTelemetry:Action": {
        lazy.SearchSERPTelemetry.reportPageAction(msg.data, browser);
        break;
      }
      case "SearchTelemetry:PageImpression": {
        lazy.SearchSERPTelemetry.reportPageImpression(msg.data, browser);
        break;
      }
      case "SearchTelemetry:Domains": {
        lazy.SearchSERPTelemetry.reportPageDomains(msg.data, browser);
        break;
      }
    }
  }
}
