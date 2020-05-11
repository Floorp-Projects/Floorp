/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

var {
  UrlbarMuxer,
  UrlbarProvider,
  UrlbarQueryContext,
  UrlbarUtils,
} = ChromeUtils.import("resource:///modules/UrlbarUtils.jsm");
XPCOMUtils.defineLazyModuleGetters(this, {
  AddonTestUtils: "resource://testing-common/AddonTestUtils.jsm",
  AppConstants: "resource://gre/modules/AppConstants.jsm",
  HttpServer: "resource://testing-common/httpd.js",
  PlacesSearchAutocompleteProvider:
    "resource://gre/modules/PlacesSearchAutocompleteProvider.jsm",
  PlacesTestUtils: "resource://testing-common/PlacesTestUtils.jsm",
  PlacesUtils: "resource://gre/modules/PlacesUtils.jsm",
  PromiseUtils: "resource://gre/modules/PromiseUtils.jsm",
  Services: "resource://gre/modules/Services.jsm",
  TestUtils: "resource://testing-common/TestUtils.jsm",
  UrlbarController: "resource:///modules/UrlbarController.jsm",
  UrlbarInput: "resource:///modules/UrlbarInput.jsm",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.jsm",
  UrlbarProviderOpenTabs: "resource:///modules/UrlbarProviderOpenTabs.jsm",
  UrlbarProvidersManager: "resource:///modules/UrlbarProvidersManager.jsm",
  UrlbarResult: "resource:///modules/UrlbarResult.jsm",
  UrlbarTestUtils: "resource://testing-common/UrlbarTestUtils.jsm",
  UrlbarTokenizer: "resource:///modules/UrlbarTokenizer.jsm",
});
const { sinon } = ChromeUtils.import("resource://testing-common/Sinon.jsm");

AddonTestUtils.init(this, false);
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "42",
  "42"
);

/**
 * @param {string} searchString The search string to insert into the context.
 * @param {object} properties Overrides for the default values.
 * @returns {UrlbarQueryContext} Creates a dummy query context with pre-filled
 *          required options.
 */
function createContext(searchString = "foo", properties = {}) {
  info(`Creating new queryContext with searchString: ${searchString}`);
  return new UrlbarQueryContext(
    Object.assign(
      {
        allowAutofill: UrlbarPrefs.get("autoFill"),
        isPrivate: true,
        maxResults: UrlbarPrefs.get("maxRichResults"),
        searchString,
      },
      properties
    )
  );
}

/**
 * Waits for the given notification from the supplied controller.
 *
 * @param {UrlbarController} controller The controller to wait for a response from.
 * @param {string} notification The name of the notification to wait for.
 * @param {boolean} expected Wether the notification is expected.
 * @returns {Promise} A promise that is resolved with the arguments supplied to
 *   the notification.
 */
function promiseControllerNotification(
  controller,
  notification,
  expected = true
) {
  return new Promise((resolve, reject) => {
    let proxifiedObserver = new Proxy(
      {},
      {
        get: (target, name) => {
          if (name == notification) {
            return (...args) => {
              controller.removeQueryListener(proxifiedObserver);
              if (expected) {
                resolve(args);
              } else {
                reject();
              }
            };
          }
          return () => false;
        },
      }
    );
    controller.addQueryListener(proxifiedObserver);
  });
}

/**
 * A basic test provider, returning all the provided matches.
 */
class TestProvider extends UrlbarTestUtils.TestProvider {
  isActive(context) {
    Assert.ok(context, "context is passed-in");
    return true;
  }
  getPriority(context) {
    Assert.ok(context, "context is passed-in");
    return 0;
  }
  async startQuery(context, add) {
    Assert.ok(context, "context is passed-in");
    Assert.equal(typeof add, "function", "add is a callback");
    this._context = context;
    for (const result of this._results) {
      add(this, result);
    }
  }
  cancelQuery(context) {
    // If the query was created but didn't run, this_context will be undefined.
    if (this._context) {
      Assert.equal(this._context, context, "cancelQuery: context is the same");
    }
    if (this._onCancel) {
      this._onCancel();
    }
  }
}

/**
 * Helper function to clear the existing providers and register a basic provider
 * that returns only the results given.
 *
 * @param {array} results The results for the provider to return.
 * @param {function} [onCancel] Optional, called when the query provider
 *                              receives a cancel instruction.
 * @param {UrlbarUtils.PROVIDER_TYPE} type The provider type.
 * @returns {string} name of the registered provider
 */
function registerBasicTestProvider(results = [], onCancel, type) {
  let provider = new TestProvider({ results, onCancel, type });
  UrlbarProvidersManager.registerProvider(provider);
  return provider.name;
}

// Creates an HTTP server for the test.
function makeTestServer(port = -1) {
  let httpServer = new HttpServer();
  httpServer.start(port);
  registerCleanupFunction(() => httpServer.stop(() => {}));
  return httpServer;
}

/**
 * Adds a search engine to the Search Service.
 *
 * @param {string} basename
 *        Basename for the engine.
 * @param {object} httpServer [optional] HTTP Server to use.
 * @returns {Promise} Resolved once the addition is complete.
 */
async function addTestEngine(basename, httpServer = undefined) {
  httpServer = httpServer || makeTestServer();
  httpServer.registerDirectory("/", do_get_cwd());
  let dataUrl =
    "http://localhost:" + httpServer.identity.primaryPort + "/data/";

  // Before initializing the search service, set the geo IP url pref to a dummy
  // string.  When the search service is initialized, it contacts the URI named
  // in this pref, causing unnecessary error logs.
  let geoPref = "browser.search.geoip.url";
  Services.prefs.setCharPref(geoPref, "");
  registerCleanupFunction(() => Services.prefs.clearUserPref(geoPref));

  info("Adding engine: " + basename);
  return new Promise(resolve => {
    Services.obs.addObserver(function obs(subject, topic, data) {
      let engine = subject.QueryInterface(Ci.nsISearchEngine);
      info("Observed " + data + " for " + engine.name);
      if (data != "engine-added" || engine.name != basename) {
        return;
      }

      Services.obs.removeObserver(obs, "browser-search-engine-modified");
      registerCleanupFunction(() => Services.search.removeEngine(engine));
      resolve(engine);
    }, "browser-search-engine-modified");

    info("Adding engine from URL: " + dataUrl + basename);
    Services.search.addEngine(dataUrl + basename, null, false);
  });
}

/**
 * Sets up a search engine that provides some suggestions by appending strings
 * onto the search query.
 *
 * @param {function} suggestionsFn
 *        A function that returns an array of suggestion strings given a
 *        search string.  If not given, a default function is used.
 * @returns {nsISearchEngine} The new engine.
 */
async function addTestSuggestionsEngine(suggestionsFn = null) {
  // This port number should match the number in engine-suggestions.xml.
  let server = makeTestServer(9000);
  server.registerPathHandler("/suggest", (req, resp) => {
    // URL query params are x-www-form-urlencoded, which converts spaces into
    // plus signs, so un-convert any plus signs back to spaces.
    let searchStr = decodeURIComponent(req.queryString.replace(/\+/g, " "));
    let suggestions = suggestionsFn
      ? suggestionsFn(searchStr)
      : [searchStr].concat(["foo", "bar"].map(s => searchStr + " " + s));
    let data = [searchStr, suggestions];
    resp.setHeader("Content-Type", "application/json", false);
    resp.write(JSON.stringify(data));
  });
  let engine = await addTestEngine("engine-suggestions.xml", server);
  return engine;
}

/**
 * Creates a UrlbarResult for a search result.
 * @param {UrlbarQueryContext} queryContext
 *   The context that this result will be displayed in.
 * @param {string} [options.suggestion]
 *   The suggestion offered by the search engine.
 * @param {string} [options.engineName]
 *   The name of the engine providing the suggestion. Leave blank if there
 *   is no suggestion.
 * @param {string} [options.query]
 *   The query that started the search. This overrides
 *   `queryContext.searchString`. This is useful when the query that will show
 *   up in the result object will be different from what was typed. For example,
 *   if a leading restriction token will be used.
 * @param {string} [options.alias]
 *   The alias for the search engine, if the search is an alias search.
 * @param {string} [options.engineIconUri]
 *   A URI for the engine's icon.
 * @param {boolean} [options.heuristic]
 *   True if this is a heuristic result. Defaults to false.
 * @param {number} [options.keywordOffer]
 *   A value from UrlbarUtils.KEYWORD_OFFER.
 * @returns {UrlbarResult}
 */
function makeSearchResult(
  queryContext,
  {
    suggestion,
    engineName,
    alias,
    query,
    engineIconUri,
    heuristic = false,
    keywordOffer,
  }
) {
  UrlbarTokenizer.tokenize(queryContext);

  if (!keywordOffer) {
    keywordOffer = UrlbarUtils.KEYWORD_OFFER.NONE;
    if (alias && !query.trim() && alias.startsWith("@")) {
      keywordOffer = heuristic
        ? UrlbarUtils.KEYWORD_OFFER.HIDE
        : UrlbarUtils.KEYWORD_OFFER.SHOW;
    }
  }

  let result = new UrlbarResult(
    UrlbarUtils.RESULT_TYPE.SEARCH,
    UrlbarUtils.RESULT_SOURCE.SEARCH,
    ...UrlbarResult.payloadAndSimpleHighlights(queryContext.tokens, {
      engine: [engineName, UrlbarUtils.HIGHLIGHT.TYPED],
      suggestion: [suggestion, UrlbarUtils.HIGHLIGHT.SUGGESTED],
      keyword: [alias, UrlbarUtils.HIGHLIGHT.TYPED],
      // Check against undefined so consumers can pass in the empty string.
      query: [
        typeof query != "undefined" ? query : queryContext.searchString.trim(),
        UrlbarUtils.HIGHLIGHT.TYPED,
      ],
      isSearchHistory: false,
      icon: [engineIconUri ? engineIconUri : ""],
      keywordOffer,
    })
  );

  result.heuristic = heuristic;
  return result;
}

/**
 * Creates a UrlbarResult for a history result.
 * @param {UrlbarQueryContext} queryContext
 *   The context that this result will be displayed in.
 * @param {string} options.title
 *   The page title.
 * @param {string} options.uri
 *   The page URI.
 * @param {array} [options.tags]
 *   An array of string tags. Defaults to an empty array.
 * @param {string} [options.iconUri]
 *   A URI for the page's icon.
 * @param {boolean} [options.heuristic]
 *   True if this is a heuristic result. Defaults to false.
 * @returns {UrlbarResult}
 */
function makeVisitResult(
  queryContext,
  { title, uri, iconUri, tags = [], heuristic = false }
) {
  UrlbarTokenizer.tokenize(queryContext);

  let payload = {
    url: [uri, UrlbarUtils.HIGHLIGHT.TYPED],
    // Check against undefined so consumers can pass in the empty string.
    icon: [typeof iconUri != "undefined" ? iconUri : `page-icon:${uri}`],
    title: [title, UrlbarUtils.HIGHLIGHT.TYPED],
  };

  if (!heuristic) {
    payload.tags = [tags, UrlbarUtils.HIGHLIGHT.TYPED];
  }

  let result = new UrlbarResult(
    UrlbarUtils.RESULT_TYPE.URL,
    UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
    ...UrlbarResult.payloadAndSimpleHighlights(queryContext.tokens, payload)
  );

  result.heuristic = heuristic;
  return result;
}

/**
 * Creates a UrlbarResult for a bookmark result.
 * @param {UrlbarQueryContext} queryContext
 *   The context that this result will be displayed in.
 * @param {string} options.title
 *   The page title.
 * @param {string} options.uri
 *   The page URI.
 * @param {string} [options.iconUri]
 *   A URI for the page's icon.
 * @param {array} [options.tags]
 *   An array of string tags. Defaults to an empty array.
 * @param {boolean} [options.heuristic]
 *   True if this is a heuristic result. Defaults to false.
 * @returns {UrlbarResult}
 */
function makeBookmarkResult(
  queryContext,
  { title, uri, iconUri, tags = [], heuristic = false }
) {
  UrlbarTokenizer.tokenize(queryContext);

  let result = new UrlbarResult(
    UrlbarUtils.RESULT_TYPE.URL,
    UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
    ...UrlbarResult.payloadAndSimpleHighlights(queryContext.tokens, {
      url: [uri, UrlbarUtils.HIGHLIGHT.TYPED],
      // Check against undefined so consumers can pass in the empty string.
      icon: [typeof iconUri != "undefined" ? iconUri : `page-icon:${uri}`],
      title: [title, UrlbarUtils.HIGHLIGHT.TYPED],
      tags: [tags, UrlbarUtils.HIGHLIGHT.TYPED],
    })
  );

  result.heuristic = heuristic;
  return result;
}

/**
 * Checks that the results returned by a UrlbarController match those in
 * the param `matches`.
 * @param {UrlbarQueryContext} context
 *   The context for this query.
 * @param {array} matches
 *   An array of UrlbarResults.
 * @param {boolean} [isPrivate]
 *   Set this to `true` to simulate a search in a private window.
 */
async function check_results({ context, matches = [] } = {}) {
  if (!context) {
    return;
  }

  // At this point frecency could still be updating due to latest pages
  // updates.
  // This is not a problem in real life, but autocomplete tests should
  // return reliable resultsets, thus we have to wait.
  await PlacesTestUtils.promiseAsyncUpdates();

  let controller = UrlbarTestUtils.newMockController({
    input: {
      isPrivate: context.isPrivate,
      window: {
        location: {
          href: AppConstants.BROWSER_CHROME_URL,
        },
      },
    },
  });
  await controller.startQuery(context);

  Assert.equal(
    context.results.length,
    matches.length,
    "Found the expected number of results."
  );

  Assert.deepEqual(
    matches.map(m => m.payload),
    context.results.map(m => m.payload),
    "Payloads are the same."
  );

  Assert.deepEqual(
    matches.map(m => m.heuristic),
    context.results.map(m => m.heuristic),
    "Heuristic results are correctly flagged."
  );
}
