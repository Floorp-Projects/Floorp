/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["BrowserSearchTelemetry"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  PartnerLinkAttribution: "resource:///modules/PartnerLinkAttribution.jsm",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.jsm",
  Services: "resource://gre/modules/Services.jsm",
  UrlbarUtils: "resource:///modules/UrlbarUtils.jsm",
});

// A list of known search origins.
const KNOWN_SEARCH_SOURCES = [
  "abouthome",
  "contextmenu",
  "newtab",
  "searchbar",
  "system",
  "urlbar",
  "urlbar-searchmode",
  "webextension",
];

/**
 * This class handles saving search telemetry related to the url bar,
 * search bar and other areas as per the sources above.
 */
class BrowserSearchTelemetryHandler {
  /**
   * Determines if we should record a search for this browser instance.
   * Private Browsing mode is normally skipped.
   *
   * @param {tabbrowser} tabbrowser
   *   The browser where the search was loaded.
   * @returns {boolean}
   *   True if the search should be recorded, false otherwise.
   */
  shouldRecordSearchCount(tabbrowser) {
    return (
      !PrivateBrowsingUtils.isWindowPrivate(tabbrowser.ownerGlobal) ||
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
    let scalarKey;
    if (searchMode.engineName) {
      let engine = Services.search.getEngineByName(searchMode.engineName);
      let resultDomain = engine.getResultDomain();
      // For built-in engines, sanitize the data in a few special cases to make
      // analysis easier.
      if (!engine.isAppProvided) {
        scalarKey = "other";
      } else if (resultDomain.includes("amazon.")) {
        // Group all the localized Amazon sites together.
        scalarKey = "Amazon";
      } else if (resultDomain.endsWith("wikipedia.org")) {
        // Group all the localized Wikipedia sites together.
        scalarKey = "Wikipedia";
      } else {
        scalarKey = searchMode.engineName;
      }
    } else if (searchMode.source) {
      scalarKey = UrlbarUtils.getResultSourceName(searchMode.source) || "other";
    }

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
   * @param {tabbrowser} tabbrowser
   *        The tabbrowser where the search was loaded.
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
   * @throws if source is not in the known sources list.
   */
  recordSearch(tabbrowser, engine, source, details = {}) {
    try {
      if (!this.shouldRecordSearchCount(tabbrowser)) {
        return;
      }
      if (!KNOWN_SEARCH_SOURCES.includes(source)) {
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
      this._handleSearchAction(engine, source, details);
    } catch (ex) {
      // Catch any errors here, so that search actions are not broken if
      // telemetry is broken for some reason.
      console.error(ex);
    }
  }

  _handleSearchAction(engine, source, details) {
    switch (source) {
      case "urlbar":
      case "searchbar":
        this._handleSearchAndUrlbar(engine, source, details);
        break;
      case "urlbar-searchmode":
        this._handleSearchAndUrlbar(engine, "urlbar_searchmode", details);
        break;
      case "abouthome":
        this._recordSearch(engine, details.url, "about_home", "enter");
        break;
      case "newtab":
        this._recordSearch(engine, details.url, "about_newtab", "enter");
        break;
      case "contextmenu":
      case "system":
      case "webextension":
        this._recordSearch(engine, details.url, source);
        break;
    }
  }

  /**
   * This function handles the "urlbar", "urlbar-oneoff", "searchbar" and
   * "searchbar-oneoff" sources.
   *
   * @param {msISearchEngine} engine
   *   The engine handling the search.
   * @param {string} source
   *   Where the search originated from.
   * @param {object} details
   *   @see recordSearch
   */
  _handleSearchAndUrlbar(engine, source, details) {
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

    this._recordSearch(engine, details.url, source, action);
  }

  _recordSearch(engine, url, source, action = null) {
    if (url) {
      PartnerLinkAttribution.makeSearchEngineRequest(engine, url).catch(
        Cu.reportError
      );
    }

    let scalarKey = action ? "search_" + action : "search";
    Services.telemetry.keyedScalarAdd(
      "browser.engagement.navigation." + source,
      scalarKey,
      1
    );
    Services.telemetry.recordEvent("navigation", "search", source, action, {
      engine: engine.telemetryId,
    });
  }
}

var BrowserSearchTelemetry = new BrowserSearchTelemetryHandler();
