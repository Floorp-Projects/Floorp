/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { BaseFeature } from "resource:///modules/urlbar/private/BaseFeature.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
  UrlbarResult: "resource:///modules/UrlbarResult.sys.mjs",
  UrlbarUtils: "resource:///modules/UrlbarUtils.sys.mjs",
});

/**
 * A feature for Yelp suggestions.
 */
export class YelpSuggestions extends BaseFeature {
  get shouldEnable() {
    return (
      lazy.UrlbarPrefs.get("yelpFeatureGate") &&
      lazy.UrlbarPrefs.get("suggest.yelp")
    );
  }

  get enablingPreferences() {
    return ["suggest.yelp"];
  }

  get rustSuggestionTypes() {
    return ["Yelp"];
  }

  getSuggestionTelemetryType(suggestion) {
    return "yelp";
  }

  makeResult(queryContext, suggestion, searchString) {
    return Object.assign(
      new lazy.UrlbarResult(
        lazy.UrlbarUtils.RESULT_TYPE.URL,
        lazy.UrlbarUtils.RESULT_SOURCE.SEARCH,
        ...lazy.UrlbarResult.payloadAndSimpleHighlights(queryContext.tokens, {
          // TODO: Should define Yelp icon here. Bug 1874624.
          url: suggestion.url,
          title: suggestion.title,
          shouldShowUrl: true,
          bottomTextL10n: { id: "firefox-suggest-yelp-bottom-text" },
        })
      ),
      {
        isBestMatch: true,
        suggestedIndex: 1,
        isRichSuggestion: true,
        richSuggestionIconSize: 24,
      }
    );
  }
}
