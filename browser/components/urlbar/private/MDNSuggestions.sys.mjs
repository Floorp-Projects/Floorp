/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { BaseFeature } from "resource:///modules/urlbar/private/BaseFeature.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  QuickSuggestRemoteSettings:
    "resource:///modules/urlbar/private/QuickSuggestRemoteSettings.sys.mjs",
  SuggestionsMap:
    "resource:///modules/urlbar/private/QuickSuggestRemoteSettings.sys.mjs",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
  UrlbarResult: "resource:///modules/UrlbarResult.sys.mjs",
  UrlbarUtils: "resource:///modules/UrlbarUtils.sys.mjs",
});

/**
 * A feature that supports MDN suggestions.
 */
export class MDNSuggestions extends BaseFeature {
  get shouldEnable() {
    return (
      lazy.UrlbarPrefs.get("mdn.featureGate") &&
      lazy.UrlbarPrefs.get("suggest.mdn") &&
      lazy.UrlbarPrefs.get("suggest.quicksuggest.nonsponsored")
    );
  }

  get enablingPreferences() {
    return [
      "mdn.featureGate",
      "suggest.mdn",
      "suggest.quicksuggest.nonsponsored",
    ];
  }

  get merinoProvider() {
    return "mdn";
  }

  enable(enabled) {
    if (enabled) {
      lazy.QuickSuggestRemoteSettings.register(this);
    } else {
      lazy.QuickSuggestRemoteSettings.unregister(this);
    }
  }

  queryRemoteSettings(searchString) {
    const suggestions = this.#suggestionsMap?.get(searchString);
    return suggestions
      ? suggestions.map(suggestion => ({ ...suggestion }))
      : [];
  }

  async onRemoteSettingsSync(rs) {
    const records = await rs.get({ filters: { type: "mdn-suggestions" } });
    if (!this.isEnabled) {
      return;
    }

    const suggestionsMap = new lazy.SuggestionsMap();

    for (const record of records) {
      const { buffer } = await rs.attachments.download(record);
      if (!this.isEnabled) {
        return;
      }

      const results = JSON.parse(new TextDecoder("utf-8").decode(buffer));
      await suggestionsMap.add(results, {
        mapKeyword:
          lazy.SuggestionsMap.MAP_KEYWORD_PREFIXES_STARTING_AT_FIRST_WORD,
      });
      if (!this.isEnabled) {
        return;
      }
    }

    this.#suggestionsMap = suggestionsMap;
  }

  async makeResult(queryContext, suggestion, searchString) {
    if (!this.isEnabled) {
      // The feature is disabled on the client, but Merino may still return
      // mdn suggestions anyway, and we filter them out here.
      return null;
    }

    const payload = {
      icon: "chrome://global/skin/icons/mdn.svg",
      url: suggestion.url,
      title: [suggestion.title, lazy.UrlbarUtils.HIGHLIGHT.TYPED],
      description: suggestion.description,
      shouldShowUrl: true,
    };

    return Object.assign(
      new lazy.UrlbarResult(
        lazy.UrlbarUtils.RESULT_TYPE.URL,
        lazy.UrlbarUtils.RESULT_SOURCE.OTHER_NETWORK,
        ...lazy.UrlbarResult.payloadAndSimpleHighlights(
          queryContext.tokens,
          payload
        )
      ),
      { showFeedbackMenu: true }
    );
  }

  #suggestionsMap = null;
}
