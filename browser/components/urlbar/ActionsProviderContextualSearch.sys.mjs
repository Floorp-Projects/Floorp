/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { UrlbarUtils } from "resource:///modules/UrlbarUtils.sys.mjs";

import {
  ActionsProvider,
  ActionsResult,
} from "resource:///modules/ActionsProvider.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  OpenSearchEngine: "resource://gre/modules/OpenSearchEngine.sys.mjs",
  loadAndParseOpenSearchEngine:
    "resource://gre/modules/OpenSearchLoader.sys.mjs",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
  UrlbarSearchUtils: "resource:///modules/UrlbarSearchUtils.sys.mjs",
});

const ENABLED_PREF = "contextualSearch.enabled";

/**
 * A provider that returns an option for using the search engine provided
 * by the active view if it utilizes OpenSearch.
 */
class ProviderContextualSearch extends ActionsProvider {
  constructor() {
    super();
    this.engines = new Map();
  }

  get name() {
    return "ActionsProviderContextualSearch";
  }

  isActive(queryContext) {
    return (
      queryContext.trimmedSearchString &&
      lazy.UrlbarPrefs.getScotchBonnetPref(ENABLED_PREF) &&
      !queryContext.searchMode
    );
  }

  async queryAction(queryContext, controller) {
    let instance = this.queryInstance;
    const hostname = URL.parse(queryContext.currentPage)?.hostname;

    // This happens on about pages, which won't have associated engines
    if (!hostname) {
      return null;
    }

    let engine = await this.fetchEngine(controller);
    let icon = engine?.icon || (await engine?.getIconURL?.());
    let defaultEngine = lazy.UrlbarSearchUtils.getDefaultEngine();

    if (
      !engine ||
      engine.name === defaultEngine?.name ||
      instance != this.queryInstance
    ) {
      return null;
    }

    return new ActionsResult({
      key: "contextual-search",
      l10nId: "urlbar-result-search-with",
      l10nArgs: { engine: engine.name || engine.title },
      icon,
    });
  }

  async fetchEngine(controller) {
    let browser = controller.browserWindow.gBrowser.selectedBrowser;
    let hostname = browser?.currentURI.host;

    if (this.engines.has(hostname)) {
      return this.engines.get(hostname);
    }

    // Strip www. to allow for partial matches when looking for an engine.
    const [host] = UrlbarUtils.stripPrefixAndTrim(hostname, {
      stripWww: true,
    });
    let engines = await lazy.UrlbarSearchUtils.enginesForDomainPrefix(host, {
      matchAllDomainLevels: true,
    });
    return engines[0] ?? browser?.engines?.[0];
  }

  async pickAction(queryContext, controller, element) {
    // If we have an engine to add, first create a new OpenSearchEngine, then
    // get and open a url to execute a search for the term in the url bar.
    let engine = await this.fetchEngine(controller);

    if (engine.uri) {
      let engineData = await lazy.loadAndParseOpenSearchEngine(
        Services.io.newURI(engine.uri)
      );
      engine = new lazy.OpenSearchEngine({ engineData });
      engine._setIcon(engine.icon, false);
    }

    const [url] = UrlbarUtils.getSearchQueryUrl(
      engine,
      queryContext.searchString
    );
    element.ownerGlobal.gBrowser.fixupAndLoadURIString(url, {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    });
    element.ownerGlobal.gBrowser.selectedBrowser.focus();
  }

  resetForTesting() {
    this.engines = new Map();
  }
}

export var ActionsProviderContextualSearch = new ProviderContextualSearch();
