/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["BrowserSearchTelemetry"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  PartnerLinkAttribution: "resource:///modules/PartnerLinkAttribution.jsm",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.jsm",
  SearchSERPTelemetry: "resource:///modules/SearchSERPTelemetry.jsm",
  UrlbarSearchUtils: "resource:///modules/UrlbarSearchUtils.jsm",
});

// A map of known search origins.
// The keys of this map are used in the calling code to recordSearch, and in
// the SEARCH_COUNTS histogram.
// The values of this map are used in the names of scalars for the following
// scalar groups:
// browser.engagement.navigation.*
// browser.search.content.*
// browser.search.withads.*
// browser.search.adclicks.*
const KNOWN_SEARCH_SOURCES = new Map([
  ["abouthome", "about_home"],
  ["contextmenu", "contextmenu"],
  ["newtab", "about_newtab"],
  ["searchbar", "searchbar"],
  ["system", "system"],
  ["urlbar", "urlbar"],
  ["urlbar-handoff", "urlbar_handoff"],
  ["urlbar-searchmode", "urlbar_searchmode"],
  ["webextension", "webextension"],
]);

/**
 * This class handles saving search telemetry related to the url bar,
 * search bar and other areas as per the sources above.
 */
class BrowserSearchTelemetryHandler {
  KNOWN_SEARCH_SOURCES = KNOWN_SEARCH_SOURCES;

  /**
   * Determines if we should record a search for this browser instance.
   * Private Browsing mode is normally skipped.
   *
   * @param {browser} browser
   *   The browser where the search was loaded.
   * @returns {boolean}
   *   True if the search should be recorded, false otherwise.
   */
  shouldRecordSearchCount(browser) {
    return (
      !lazy.PrivateBrowsingUtils.isWindowPrivate(browser.ownerGlobal) ||
      !Services.prefs.getBoolPref("browser.engagement.search_counts.pbm", false)
    );
  }

  /**
   * Records the method by which the user selected a result from the urlbar or
   * searchbar.
   *
   * @param {Event} event
   *        The event that triggered the selection.
   * @param {string} source
   *        Either "urlbar" or "searchbar" depending on the source.
   * @param {number} index
   *        The index that the user chose in the popup, or -1 if there wasn't a
   *        selection.
   * @param {string} userSelectionBehavior
   *        How the user cycled through results before picking the current match.
   *        Could be one of "tab", "arrow" or "none".
   */
  recordSearchSuggestionSelectionMethod(
    event,
    source,
    index,
    userSelectionBehavior = "none"
  ) {
    // If the contents of the histogram are changed then
    // `UrlbarTestUtils.SELECTED_RESULT_METHODS` should also be updated.
    if (source == "searchbar" && userSelectionBehavior != "none") {
      throw new Error("Did not expect a selection behavior for the searchbar.");
    }

    let histogram = Services.telemetry.getHistogramById(
      source == "urlbar"
        ? "FX_URLBAR_SELECTED_RESULT_METHOD"
        : "FX_SEARCHBAR_SELECTED_RESULT_METHOD"
    );
    // command events are from the one-off context menu.  Treat them as clicks.
    // Note that we don't care about MouseEvent subclasses here, since
    // those are not clicks.
    let isClick =
      event &&
      (ChromeUtils.getClassName(event) == "MouseEvent" ||
        event.type == "command");
    let category;
    if (isClick) {
      category = "click";
    } else if (index >= 0) {
      switch (userSelectionBehavior) {
        case "tab":
          category = "tabEnterSelection";
          break;
        case "arrow":
          category = "arrowEnterSelection";
          break;
        case "rightClick":
          // Selected by right mouse button.
          category = "rightClickEnter";
          break;
        default:
          category = "enterSelection";
      }
    } else {
      category = "enter";
    }
    histogram.add(category);
  }

  /**
   * Records entry into the Urlbar's search mode.
   *
   * Telemetry records only which search mode is entered and how it was entered.
   * It does not record anything pertaining to searches made within search mode.
   * @param {object} searchMode
   *   A search mode object. See UrlbarInput.setSearchMode documentation for
   *   details.
   */
  recordSearchMode(searchMode) {
    // Search mode preview is not search mode. Recording it would just create
    // noise.
    if (searchMode.isPreview) {
      return;
    }

    let scalarKey = lazy.UrlbarSearchUtils.getSearchModeScalarKey(searchMode);
    Services.telemetry.keyedScalarAdd(
      "urlbar.searchmode." + searchMode.entry,
      scalarKey,
      1
    );
  }

  /**
   * The main entry point for recording search related Telemetry. This includes
   * search counts and engagement measurements.
   *
   * Telemetry records only search counts per engine and action origin, but
   * nothing pertaining to the search contents themselves.
   *
   * @param {browser} browser
   *        The browser where the search originated.
   * @param {nsISearchEngine} engine
   *        The engine handling the search.
   * @param {string} source
   *        Where the search originated from. See KNOWN_SEARCH_SOURCES for allowed
   *        values.
   * @param {object} [details] Options object.
   * @param {boolean} [details.isOneOff=false]
   *        true if this event was generated by a one-off search.
   * @param {boolean} [details.isSuggestion=false]
   *        true if this event was generated by a suggested search.
   * @param {boolean} [details.isFormHistory=false]
   *        true if this event was generated by a form history result.
   * @param {string} [details.alias=null]
   *        The search engine alias used in the search, if any.
   * @param {string} [details.newtabSessionId=undefined]
   *        The newtab session that prompted this search, if any.
   * @throws if source is not in the known sources list.
   */
  recordSearch(browser, engine, source, details = {}) {
    try {
      if (!this.shouldRecordSearchCount(browser)) {
        return;
      }
      if (!KNOWN_SEARCH_SOURCES.has(source)) {
        console.error("Unknown source for search: ", source);
        return;
      }

      const countIdPrefix = `${engine.telemetryId}.`;
      const countIdSource = countIdPrefix + source;
      let histogram = Services.telemetry.getKeyedHistogramById("SEARCH_COUNTS");

      if (
        details.alias &&
        engine.isAppProvided &&
        engine.aliases.includes(details.alias)
      ) {
        // This is a keyword search using an AppProvided engine.
        // Record the source as "alias", not "urlbar".
        histogram.add(countIdPrefix + "alias");
      } else {
        histogram.add(countIdSource);
      }

      // Dispatch the search signal to other handlers.
      switch (source) {
        case "urlbar":
        case "searchbar":
        case "urlbar-searchmode":
        case "urlbar-handoff":
          this._handleSearchAndUrlbar(browser, engine, source, details);
          break;
        case "abouthome":
        case "newtab":
          this._recordSearch(browser, engine, details.url, source, "enter");
          break;
        default:
          this._recordSearch(browser, engine, details.url, source);
          break;
      }
      if (["urlbar-handoff", "abouthome", "newtab"].includes(source)) {
        Glean.newtabSearch.issued.record({
          newtab_session_id: details.newtabSessionId,
          search_access_point: source,
          telemetry_id: engine.telemetryId,
        });
        lazy.SearchSERPTelemetry.recordBrowserNewtabSession(
          browser,
          details.newtabSessionId
        );
      }
    } catch (ex) {
      // Catch any errors here, so that search actions are not broken if
      // telemetry is broken for some reason.
      console.error(ex);
    }
  }

  /**
   * This function handles the "urlbar", "urlbar-oneoff", "searchbar" and
   * "searchbar-oneoff" sources.
   *
   * @param {browser} browser
   *   The browser where the search originated.
   * @param {nsISearchEngine} engine
   *   The engine handling the search.
   * @param {string} source
   *   Where the search originated from.
   * @param {object} details
   *   @see recordSearch
   */
  _handleSearchAndUrlbar(browser, engine, source, details) {
    const isOneOff = !!details.isOneOff;
    let action = "enter";
    if (isOneOff) {
      action = "oneoff";
    } else if (details.isFormHistory) {
      action = "formhistory";
    } else if (details.isSuggestion) {
      action = "suggestion";
    } else if (details.alias) {
      action = "alias";
    }

    this._recordSearch(browser, engine, details.url, source, action);
  }

  _recordSearch(browser, engine, url, source, action = null) {
    if (url) {
      lazy.PartnerLinkAttribution.makeSearchEngineRequest(engine, url).catch(
        Cu.reportError
      );
    }

    let scalarSource = KNOWN_SEARCH_SOURCES.get(source);

    lazy.SearchSERPTelemetry.recordBrowserSource(browser, scalarSource);

    let scalarKey = action ? "search_" + action : "search";
    Services.telemetry.keyedScalarAdd(
      "browser.engagement.navigation." + scalarSource,
      scalarKey,
      1
    );
    Services.telemetry.recordEvent(
      "navigation",
      "search",
      scalarSource,
      action,
      {
        engine: engine.telemetryId,
      }
    );
  }
}

var BrowserSearchTelemetry = new BrowserSearchTelemetryHandler();
