/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This module exports a provider that offers a search engine when the user is
 * typing a search engine domain.
 */

var EXPORTED_SYMBOLS = ["UrlbarProviderTabToSearch"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
XPCOMUtils.defineLazyModuleGetters(this, {
  Services: "resource://gre/modules/Services.jsm",
  UrlbarView: "resource:///modules/UrlbarView.jsm",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.jsm",
  UrlbarProvider: "resource:///modules/UrlbarUtils.jsm",
  UrlbarProviderAutofill: "resource:///modules/UrlbarProviderAutofill.jsm",
  UrlbarResult: "resource:///modules/UrlbarResult.jsm",
  UrlbarSearchUtils: "resource:///modules/UrlbarSearchUtils.jsm",
  UrlbarTokenizer: "resource:///modules/UrlbarTokenizer.jsm",
  UrlbarUtils: "resource:///modules/UrlbarUtils.jsm",
});

const DYNAMIC_RESULT_TYPE = "onboardTabToSearch";
const VIEW_TEMPLATE = {
  attributes: {
    role: "group",
    selectable: "true",
  },
  children: [
    {
      name: "no-wrap",
      tag: "span",
      classList: ["urlbarView-no-wrap"],
      children: [
        {
          name: "icon",
          tag: "img",
          classList: ["urlbarView-favicon"],
        },
        {
          name: "text-container",
          tag: "span",
          children: [
            {
              name: "first-row-container",
              tag: "span",
              children: [
                {
                  name: "title",
                  tag: "span",
                  classList: ["urlbarView-title"],
                  children: [
                    {
                      name: "titleStrong",
                      tag: "strong",
                    },
                  ],
                },
                {
                  name: "title-separator",
                  tag: "span",
                  classList: ["urlbarView-title-separator"],
                },
                {
                  name: "action",
                  tag: "span",
                  classList: ["urlbarView-action"],
                  attributes: {
                    "slide-in": true,
                  },
                },
              ],
            },
            {
              name: "description",
              tag: "span",
            },
          ],
        },
      ],
    },
  ],
};

/**
 * Initializes this provider's dynamic result. To be called after the creation
 *  of the provider singleton.
 */
function initializeDynamicResult() {
  UrlbarResult.addDynamicResultType(DYNAMIC_RESULT_TYPE);
  UrlbarView.addDynamicViewTemplate(DYNAMIC_RESULT_TYPE, VIEW_TEMPLATE);
}

/**
 * Class used to create the provider.
 */
class ProviderTabToSearch extends UrlbarProvider {
  constructor() {
    super();
    this.enginesShown = {
      onboarding: new Set(),
      regular: new Set(),
    };
  }

  /**
   * Returns the name of this provider.
   * @returns {string} the name of this provider.
   */
  get name() {
    return "TabToSearch";
  }

  /**
   * Returns the type of this provider.
   * @returns {integer} one of the types from UrlbarUtils.PROVIDER_TYPE.*
   */
  get type() {
    return UrlbarUtils.PROVIDER_TYPE.PROFILE;
  }

  /**
   * Whether this provider should be invoked for the given context.
   * If this method returns false, the providers manager won't start a query
   * with this provider, to save on resources.
   * @param {UrlbarQueryContext} queryContext The query context object
   * @returns {boolean} Whether this provider should be invoked for the search.
   */
  async isActive(queryContext) {
    return (
      queryContext.searchString &&
      queryContext.tokens.length == 1 &&
      !queryContext.searchMode &&
      UrlbarPrefs.get("suggest.engines")
    );
  }

  /**
   * Gets the provider's priority.
   * @param {UrlbarQueryContext} queryContext The query context object
   * @returns {number} The provider's priority for the given query.
   */
  getPriority(queryContext) {
    return 0;
  }

  /**
   * This is called only for dynamic result types, when the urlbar view updates
   * the view of one of the results of the provider.  It should return an object
   * describing the view update.
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
          src: result.payload.icon,
        },
      },
      titleStrong: {
        l10n: {
          id: "urlbar-result-action-search-w-engine",
          args: {
            engine: result.payload.engine,
          },
        },
      },
      action: {
        l10n: {
          id: result.payload.isGeneralPurposeEngine
            ? "urlbar-result-action-tabtosearch-web"
            : "urlbar-result-action-tabtosearch-other-engine",
          args: {
            engine: result.payload.engine,
          },
        },
      },
      description: {
        l10n: {
          id: "urlbar-tabtosearch-onboard",
        },
      },
    };
  }

  /**
   * Called when any selectable element in a dynamic result's view is picked.
   *
   * @param {UrlbarResult} result
   *   The result that was picked.
   * @param {Element} element
   *   The element in the result's view that was picked.
   */
  pickResult(result, element) {
    element.ownerGlobal.gURLBar.maybeConfirmSearchModeFromResult({
      result,
      checkValue: false,
    });
  }

  /**
   * Called when a result from the provider is selected. "Selected" refers to
   * the user highlighing the result with the arrow keys/Tab, before it is
   * picked. onSelection is also called when a user clicks a result. In the
   * event of a click, onSelection is called just before pickResult.
   *
   * @param {UrlbarResult} result
   *   The result that was selected.
   * @param {Element} element
   *   The element in the result's view that was selected.
   */
  onSelection(result, element) {
    // We keep track of the number of times the user interacts with
    // tab-to-search onboarding results so we stop showing them after
    // `tabToSearch.onboard.interactionsLeft` interactions.
    // Also do not increment the counter if the result was interacted with less
    // than 5 minutes ago. This is a guard against the user running up the
    // counter by interacting with the same result repeatedly.
    if (
      result.payload.dynamicType &&
      (!this.onboardingInteractionAtTime ||
        this.onboardingInteractionAtTime < Date.now() - 1000 * 60 * 5)
    ) {
      let interactionsLeft = UrlbarPrefs.get(
        "tabToSearch.onboard.interactionsLeft"
      );

      if (interactionsLeft > 0) {
        UrlbarPrefs.set(
          "tabToSearch.onboard.interactionsLeft",
          --interactionsLeft
        );
      }

      this.onboardingInteractionAtTime = Date.now();
    }
  }

  /**
   * Called when the user starts and ends an engagement with the urlbar.  For
   * details on parameters, see UrlbarProvider.onEngagement().
   *
   * @param {boolean} isPrivate
   *   True if the engagement is in a private context.
   * @param {string} state
   *   The state of the engagement, one of: start, engagement, abandonment,
   *   discard
   * @param {UrlbarQueryContext} queryContext
   *   The engagement's query context.  This is *not* guaranteed to be defined
   *   when `state` is "start".  It will always be defined for "engagement" and
   *   "abandonment".
   * @param {object} details
   *   This is defined only when `state` is "engagement" or "abandonment", and
   *   it describes the search string and picked result.
   */
  onEngagement(isPrivate, state, queryContext, details) {
    if (!this.enginesShown.regular.size && !this.enginesShown.onboarding.size) {
      return;
    }

    try {
      // urlbar.tabtosearch.* is prerelease-only/opt-in for now. See bug 1686330.
      for (let engine of this.enginesShown.regular) {
        let scalarKey = UrlbarSearchUtils.getSearchModeScalarKey({
          engineName: engine,
        });
        Services.telemetry.keyedScalarAdd(
          "urlbar.tabtosearch.impressions",
          scalarKey,
          1
        );
      }
      for (let engine of this.enginesShown.onboarding) {
        let scalarKey = UrlbarSearchUtils.getSearchModeScalarKey({
          engineName: engine,
        });
        Services.telemetry.keyedScalarAdd(
          "urlbar.tabtosearch.impressions_onboarding",
          scalarKey,
          1
        );
      }

      // We also record in urlbar.tips because only it has been approved for use
      // in release channels.
      Services.telemetry.keyedScalarAdd(
        "urlbar.tips",
        "tabtosearch-shown",
        this.enginesShown.regular.size
      );
      Services.telemetry.keyedScalarAdd(
        "urlbar.tips",
        "tabtosearch_onboard-shown",
        this.enginesShown.onboarding.size
      );
    } catch (ex) {
      // If your test throws this error or causes another test to throw it, it
      // is likely because your test showed a tab-to-search result but did not
      // start and end the engagement in which it was shown. Be sure to fire an
      // input event to start an engagement and blur the Urlbar to end it.
      Cu.reportError(`Exception while recording TabToSearch telemetry: ${ex})`);
    } finally {
      // Even if there's an exception, we want to clear these Sets. Otherwise,
      // we might get into a state where we repeatedly run the same engines
      // through the code above and never record telemetry, because there's an
      // error every time.
      this.enginesShown.regular.clear();
      this.enginesShown.onboarding.clear();
    }
  }

  /**
   * Defines whether the view should defer user selection events while waiting
   * for the first result from this provider.
   * @returns {boolean} Whether the provider wants to defer user selection
   *          events.
   */
  get deferUserSelection() {
    return true;
  }

  /**
   * Starts querying.
   * @param {object} queryContext The query context object
   * @param {function} addCallback Callback invoked by the provider to add a new
   *        result.
   * @returns {Promise} resolved when the query stops.
   */
  async startQuery(queryContext, addCallback) {
    // enginesForDomainPrefix only matches against engine domains.
    // Remove trailing slashes and www. from the search string and check if the
    // resulting string is worth matching.
    let [searchStr] = UrlbarUtils.stripPrefixAndTrim(
      queryContext.searchString,
      {
        stripWww: true,
        trimSlash: true,
      }
    );
    // Skip any string that cannot be an origin.
    if (
      !UrlbarTokenizer.looksLikeOrigin(searchStr, {
        ignoreKnownDomains: true,
        noIp: true,
      })
    ) {
      return;
    }

    // Also remove the public suffix, if present, to allow for partial matches.
    if (searchStr.includes(".")) {
      searchStr = UrlbarUtils.stripPublicSuffixFromHost(searchStr);
    }

    // Add all matching engines.
    let engines = await UrlbarSearchUtils.enginesForDomainPrefix(searchStr, {
      matchAllDomainLevels: true,
      onlyEnabled: true,
    });
    if (!engines.length) {
      return;
    }

    const onboardingInteractionsLeft = UrlbarPrefs.get(
      "tabToSearch.onboard.interactionsLeft"
    );

    // If the engine host begins with the search string, autofill may happen
    // for it, and the Muxer will retain the result only if there's a matching
    // autofill heuristic result.
    // Otherwise, we may have a partial match, where the search string is at
    // the boundary of a host part, for example "wiki" in "en.wikipedia.org".
    // We put those engines apart, and later we check if their host satisfies
    // the autofill threshold. If they do, we mark them with the
    // "satisfiesAutofillThreshold" payload property, so the muxer can avoid
    // filtering them out.
    let partialMatchEnginesByHost = new Map();

    for (let engine of engines) {
      // Trim the engine host. This will also be set as the result url, so the
      // Muxer can use it to filter.
      let [host] = UrlbarUtils.stripPrefixAndTrim(engine.getResultDomain(), {
        stripWww: true,
      });
      // Check if the host may be autofilled.
      if (host.startsWith(searchStr.toLocaleLowerCase())) {
        if (onboardingInteractionsLeft > 0) {
          addCallback(this, makeOnboardingResult(engine));
        } else {
          addCallback(this, makeResult(queryContext, engine));
        }
        continue;
      }

      // Otherwise it may be a partial match that would not be autofilled.
      if (host.includes("." + searchStr.toLocaleLowerCase())) {
        partialMatchEnginesByHost.set(engine.getResultDomain(), engine);
        // Don't continue here, we are looking for more partial matches.
      }
      // We also try to match the searchForm domain, because otherwise for an
      // engine like ebay, we'd check rover.ebay.com, when the user is likely
      // to visit ebay.LANG. The searchForm URL often points to the main host.
      let searchFormHost;
      try {
        searchFormHost = new URL(engine.searchForm).host;
      } catch (ex) {
        // Invalid url or no searchForm.
      }
      if (searchFormHost?.includes("." + searchStr)) {
        partialMatchEnginesByHost.set(searchFormHost, engine);
      }
    }
    if (partialMatchEnginesByHost.size) {
      let host = await UrlbarProviderAutofill.getTopHostOverThreshold(
        queryContext,
        Array.from(partialMatchEnginesByHost.keys())
      );
      if (host) {
        let engine = partialMatchEnginesByHost.get(host);
        if (onboardingInteractionsLeft > 0) {
          addCallback(this, makeOnboardingResult(engine, true));
        } else {
          addCallback(this, makeResult(queryContext, engine, true));
        }
      }
    }
  }
}

function makeOnboardingResult(engine, satisfiesAutofillThreshold = false) {
  let [url] = UrlbarUtils.stripPrefixAndTrim(engine.getResultDomain(), {
    stripWww: true,
  });
  url = url.substr(0, url.length - engine.searchUrlPublicSuffix.length);
  let result = new UrlbarResult(
    UrlbarUtils.RESULT_TYPE.DYNAMIC,
    UrlbarUtils.RESULT_SOURCE.SEARCH,
    {
      engine: engine.name,
      url,
      providesSearchMode: true,
      icon: UrlbarUtils.ICON.SEARCH_GLASS,
      dynamicType: DYNAMIC_RESULT_TYPE,
      satisfiesAutofillThreshold,
    }
  );
  result.resultSpan = 2;
  result.suggestedIndex = 1;
  return result;
}

function makeResult(context, engine, satisfiesAutofillThreshold = false) {
  let [url] = UrlbarUtils.stripPrefixAndTrim(engine.getResultDomain(), {
    stripWww: true,
  });
  url = url.substr(0, url.length - engine.searchUrlPublicSuffix.length);
  let result = new UrlbarResult(
    UrlbarUtils.RESULT_TYPE.SEARCH,
    UrlbarUtils.RESULT_SOURCE.SEARCH,
    ...UrlbarResult.payloadAndSimpleHighlights(context.tokens, {
      engine: engine.name,
      isGeneralPurposeEngine: engine.isGeneralPurposeEngine,
      url,
      providesSearchMode: true,
      icon: UrlbarUtils.ICON.SEARCH_GLASS,
      query: "",
      satisfiesAutofillThreshold,
    })
  );
  result.suggestedIndex = 1;
  return result;
}

var UrlbarProviderTabToSearch = new ProviderTabToSearch();
initializeDynamicResult();
