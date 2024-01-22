/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  UrlbarProvider,
  UrlbarUtils,
} from "resource:///modules/UrlbarUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.sys.mjs",
  OpenSearchEngine: "resource://gre/modules/OpenSearchEngine.sys.mjs",
  loadAndParseOpenSearchEngine:
    "resource://gre/modules/OpenSearchLoader.sys.mjs",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
  UrlbarResult: "resource:///modules/UrlbarResult.sys.mjs",
  UrlbarSearchUtils: "resource:///modules/UrlbarSearchUtils.sys.mjs",
  UrlbarView: "resource:///modules/UrlbarView.sys.mjs",
});

const DYNAMIC_RESULT_TYPE = "contextualSearch";

const ENABLED_PREF = "contextualSearch.enabled";

const VIEW_TEMPLATE = {
  attributes: {
    selectable: true,
  },
  children: [
    {
      name: "no-wrap",
      tag: "span",
      classList: ["urlbarView-no-wrap", "urlbarView-overflowable"],
      children: [
        {
          name: "icon",
          tag: "img",
          classList: ["urlbarView-favicon"],
        },
        {
          name: "search",
          tag: "span",
          classList: ["urlbarView-title", "urlbarView-overflowable"],
        },
        {
          name: "separator",
          tag: "span",
          classList: ["urlbarView-title-separator"],
        },
        {
          name: "description",
          tag: "span",
        },
      ],
    },
  ],
};

/**
 * A provider that returns an option for using the search engine provided
 * by the active view if it utilizes OpenSearch.
 */
class ProviderContextualSearch extends UrlbarProvider {
  constructor() {
    super();
    this.engines = new Map();
    lazy.UrlbarResult.addDynamicResultType(DYNAMIC_RESULT_TYPE);
    lazy.UrlbarView.addDynamicViewTemplate(DYNAMIC_RESULT_TYPE, VIEW_TEMPLATE);
  }

  /**
   * Unique name for the provider, used by the context to filter on providers.
   * Not using a unique name will cause the newest registration to win.
   *
   * @returns {string}
   */
  get name() {
    return "UrlbarProviderContextualSearch";
  }

  /**
   * The type of the provider.
   *
   * @returns {UrlbarUtils.PROVIDER_TYPE}
   */
  get type() {
    return UrlbarUtils.PROVIDER_TYPE.PROFILE;
  }

  /**
   * Whether this provider should be invoked for the given context.
   * If this method returns false, the providers manager won't start a query
   * with this provider, to save on resources.
   *
   * @param {UrlbarQueryContext} queryContext The query context object
   * @returns {boolean} Whether this provider should be invoked for the search.
   */
  isActive(queryContext) {
    return (
      queryContext.trimmedSearchString &&
      !queryContext.searchMode &&
      lazy.UrlbarPrefs.get(ENABLED_PREF)
    );
  }

  /**
   * Starts querying. Extended classes should return a Promise resolved when the
   * provider is done searching AND returning results.
   *
   * @param {UrlbarQueryContext} queryContext The query context object
   * @param {Function} addCallback Callback invoked by the provider to add a new
   *        result. A UrlbarResult should be passed to it.
   */
  async startQuery(queryContext, addCallback) {
    let engine;
    const hostname =
      queryContext?.currentPage && new URL(queryContext.currentPage).hostname;

    // This happens on about pages, which won't have associated engines
    if (!hostname) {
      return;
    }

    // First check to see if there's a cached search engine for the host.
    // If not, check to see if an installed engine matches the current view.
    if (this.engines.has(hostname)) {
      engine = this.engines.get(hostname);
    } else {
      // Strip www. to allow for partial matches when looking for an engine.
      const [host] = UrlbarUtils.stripPrefixAndTrim(hostname, {
        stripWww: true,
      });
      engine = (
        await lazy.UrlbarSearchUtils.enginesForDomainPrefix(host, {
          matchAllDomainLevels: true,
          onlyEnabled: false,
        })
      )[0];
    }

    if (engine) {
      this.engines.set(hostname, engine);
      // Check to see if the engine that was found is the default engine.
      // The default engine will often be used to populate the heuristic result,
      // and we want to avoid ending up with two nearly identical search results.
      let defaultEngine = lazy.UrlbarSearchUtils.getDefaultEngine();
      if (engine.name === defaultEngine?.name) {
        return;
      }
      const [url] = UrlbarUtils.getSearchQueryUrl(
        engine,
        queryContext.searchString
      );
      let result = this.makeResult({
        url,
        engine: engine.name,
        icon: engine.getIconURL(),
        input: queryContext.searchString,
        shouldNavigate: true,
      });
      addCallback(this, result);
      return;
    }

    // If the current view has engines that haven't been added, return a result
    // that will first add an engine, then use it to search.
    let window = lazy.BrowserWindowTracker.getTopWindow();
    let engineToAdd = window?.gBrowser.selectedBrowser?.engines?.[0];

    if (engineToAdd) {
      let result = this.makeResult({
        hostname,
        url: engineToAdd.uri,
        engine: engineToAdd.title,
        icon: engineToAdd.icon,
        input: queryContext.searchString,
        shouldAddEngine: true,
      });
      addCallback(this, result);
    }
  }

  makeResult({
    engine,
    icon,
    url,
    input,
    hostname,
    shouldNavigate = false,
    shouldAddEngine = false,
  }) {
    let result = new lazy.UrlbarResult(
      UrlbarUtils.RESULT_TYPE.DYNAMIC,
      UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
      {
        engine,
        icon,
        url,
        input,
        hostname,
        shouldAddEngine,
        shouldNavigate,
        dynamicType: DYNAMIC_RESULT_TYPE,
      }
    );
    result.suggestedIndex = -1;
    return result;
  }

  /**
   * This is called when the urlbar view updates the view of one of the results
   * of the provider.  It should return an object describing the view update.
   * See the base UrlbarProvider class for more.
   *
   * @param {UrlbarResult} result The result whose view will be updated.
   * @param {Map} idsByName
   *   A Map from an element's name, as defined by the provider; to its ID in
   *   the DOM, as defined by the browser.
   * @returns {object} An object describing the view update.
   */
  getViewUpdate(result, idsByName) {
    return {
      icon: {
        attributes: {
          src: result.payload.icon || UrlbarUtils.ICON.SEARCH_GLASS,
        },
      },
      search: {
        textContent: result.payload.input,
        attributes: {
          title: result.payload.input,
        },
      },
      description: {
        l10n: {
          id: "urlbar-result-action-search-w-engine",
          args: {
            engine: result.payload.engine,
          },
        },
      },
    };
  }

  onEngagement(state, queryContext, details, controller) {
    let { result } = details;
    if (result?.providerName == this.name) {
      this.#pickResult(result, controller.browserWindow);
    }
  }

  async #pickResult(result, window) {
    // If we have an engine to add, first create a new OpenSearchEngine, then
    // get and open a url to execute a search for the term in the url bar.
    // In cases where we don't have to create a new engine, navigation is
    // handled automatically by providing `shouldNavigate: true` in the result.
    if (result.payload.shouldAddEngine) {
      let engineData = await lazy.loadAndParseOpenSearchEngine(
        Services.io.newURI(result.payload.url)
      );
      let newEngine = new lazy.OpenSearchEngine({ engineData });
      newEngine._setIcon(result.payload.icon, false);
      this.engines.set(result.payload.hostname, newEngine);
      const [url] = UrlbarUtils.getSearchQueryUrl(
        newEngine,
        result.payload.input
      );
      window.gBrowser.fixupAndLoadURIString(url, {
        triggeringPrincipal:
          Services.scriptSecurityManager.getSystemPrincipal(),
      });
    }
  }
}

export var UrlbarProviderContextualSearch = new ProviderContextualSearch();
