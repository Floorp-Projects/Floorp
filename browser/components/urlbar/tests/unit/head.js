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
  PlacesTestUtils: "resource://testing-common/PlacesTestUtils.jsm",
  PlacesUtils: "resource://gre/modules/PlacesUtils.jsm",
  PromiseUtils: "resource://gre/modules/PromiseUtils.jsm",
  SearchTestUtils: "resource://testing-common/SearchTestUtils.jsm",
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

XPCOMUtils.defineLazyGetter(this, "QuickSuggestTestUtils", () => {
  const { QuickSuggestTestUtils: module } = ChromeUtils.import(
    "resource://testing-common/QuickSuggestTestUtils.jsm"
  );
  module.init(this);
  return module;
});

SearchTestUtils.init(this);
AddonTestUtils.init(this, false);
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "42",
  "42"
);

const SUGGESTIONS_ENGINE_NAME = "Suggestions";
const TAIL_SUGGESTIONS_ENGINE_NAME = "Tail Suggestions";

add_task(async function initXPCShellDependencies() {
  await UrlbarTestUtils.initXPCShellDependencies();
});

/**
 * Gets the database connection.  If the Places connection is invalid it will
 * try to create a new connection.
 *
 * @param [optional] aForceNewConnection
 *        Forces creation of a new connection to the database.  When a
 *        connection is asyncClosed it cannot anymore schedule async statements,
 *        though connectionReady will keep returning true (Bug 726990).
 *
 * @return The database connection or null if unable to get one.
 */
var gDBConn;
function DBConn(aForceNewConnection) {
  if (!aForceNewConnection) {
    let db = PlacesUtils.history.DBConnection;
    if (db.connectionReady) {
      return db;
    }
  }

  // If the Places database connection has been closed, create a new connection.
  if (!gDBConn || aForceNewConnection) {
    let file = Services.dirsvc.get("ProfD", Ci.nsIFile);
    file.append("places.sqlite");
    let dbConn = (gDBConn = Services.storage.openDatabase(file));

    TestUtils.topicObserved("profile-before-change").then(() =>
      dbConn.asyncClose()
    );
  }

  return gDBConn.connectionReady ? gDBConn : null;
}

/**
 * @param {string} searchString The search string to insert into the context.
 * @param {object} properties Overrides for the default values.
 * @returns {UrlbarQueryContext} Creates a dummy query context with pre-filled
 *          required options.
 */
function createContext(searchString = "foo", properties = {}) {
  info(`Creating new queryContext with searchString: ${searchString}`);
  let context = new UrlbarQueryContext(
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
  UrlbarTokenizer.tokenize(context);
  return context;
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
    // If the query was created but didn't run, this._context will be undefined.
    if (this._context) {
      Assert.equal(this._context, context, "cancelQuery: context is the same");
    }
    if (this._onCancel) {
      this._onCancel();
    }
  }
}

function convertToUtf8(str) {
  return String.fromCharCode(...new TextEncoder().encode(str));
}

/**
 * Helper function to clear the existing providers and register a basic provider
 * that returns only the results given.
 *
 * @param {array} results The results for the provider to return.
 * @param {function} [onCancel] Optional, called when the query provider
 *                              receives a cancel instruction.
 * @param {UrlbarUtils.PROVIDER_TYPE} type The provider type.
 * @param {string} [name] Optional, use as the provider name.
 *                        If none, a default name is chosen.
 * @returns {UrlbarProvider} The provider
 */
function registerBasicTestProvider(results = [], onCancel, type, name) {
  let provider = new TestProvider({ results, onCancel, type, name });
  UrlbarProvidersManager.registerProvider(provider);
  registerCleanupFunction(() =>
    UrlbarProvidersManager.unregisterProvider(provider)
  );
  return provider;
}

// Creates an HTTP server for the test.
function makeTestServer(port = -1) {
  let httpServer = new HttpServer();
  httpServer.start(port);
  registerCleanupFunction(() => httpServer.stop(() => {}));
  return httpServer;
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
  let server = makeTestServer();
  server.registerPathHandler("/suggest", (req, resp) => {
    let params = new URLSearchParams(req.queryString);
    let searchStr = params.get("q");
    let suggestions = suggestionsFn
      ? suggestionsFn(searchStr)
      : [searchStr].concat(["foo", "bar"].map(s => searchStr + " " + s));
    let data = [searchStr, suggestions];
    resp.setHeader("Content-Type", "application/json", false);
    resp.write(JSON.stringify(data));
  });
  await SearchTestUtils.installSearchExtension({
    name: SUGGESTIONS_ENGINE_NAME,
    search_url: `http://localhost:${server.identity.primaryPort}/search`,
    suggest_url: `http://localhost:${server.identity.primaryPort}/suggest`,
    suggest_url_get_params: "?q={searchTerms}",
    // test_search_suggestions_aliases.js uses the search form.
    search_form: `http://localhost:${server.identity.primaryPort}/search?q={searchTerms}`,
  });
  let engine = Services.search.getEngineByName("Suggestions");
  return engine;
}

/**
 * Sets up a search engine that provides some tail suggestions by creating an
 * array that mimics Google's tail suggestion responses.
 *
 * @param {function} suggestionsFn
 *        A function that returns an array that mimics Google's tail suggestion
 *        responses. See bug 1626897.
 *        NOTE: Consumers specifying suggestionsFn must include searchStr as a
 *              part of the array returned by suggestionsFn.
 * @returns {nsISearchEngine} The new engine.
 */
async function addTestTailSuggestionsEngine(suggestionsFn = null) {
  // This port number should match the number in engine-tail-suggestions.xml.
  let server = makeTestServer();
  server.registerPathHandler("/suggest", (req, resp) => {
    let params = new URLSearchParams(req.queryString);
    let searchStr = params.get("q");
    let suggestions = suggestionsFn
      ? suggestionsFn(searchStr)
      : [
          "what time is it in t",
          ["what is the time today texas"].concat(
            ["toronto", "tunisia"].map(s => searchStr + s.slice(1))
          ),
          [],
          {
            "google:irrelevantparameter": [],
            "google:suggestdetail": [{}].concat(
              ["toronto", "tunisia"].map(s => ({
                mp: "… ",
                t: s,
              }))
            ),
          },
        ];
    let data = suggestions;
    let jsonString = JSON.stringify(data);
    // This script must be evaluated as UTF-8 for this to write out the bytes of
    // the string in UTF-8.  If it's evaluated as Latin-1, the written bytes
    // will be the result of UTF-8-encoding the result-string *twice*, which
    // will break the "… " match prefixes.
    let stringOfUtf8Bytes = convertToUtf8(jsonString);
    resp.setHeader("Content-Type", "application/json", false);
    resp.write(stringOfUtf8Bytes);
  });
  await SearchTestUtils.installSearchExtension({
    name: TAIL_SUGGESTIONS_ENGINE_NAME,
    search_url: `http://localhost:${server.identity.primaryPort}/search`,
    suggest_url: `http://localhost:${server.identity.primaryPort}/suggest`,
    suggest_url_get_params: "?q={searchTerms}",
  });
  let engine = Services.search.getEngineByName("Tail Suggestions");
  return engine;
}

async function addOpenPages(uri, count = 1, userContextId = 0) {
  for (let i = 0; i < count; i++) {
    await UrlbarProviderOpenTabs.registerOpenTab(
      uri.spec,
      userContextId,
      false
    );
  }
}

async function removeOpenPages(aUri, aCount = 1, aUserContextId = 0) {
  for (let i = 0; i < aCount; i++) {
    await UrlbarProviderOpenTabs.unregisterOpenTab(
      aUri.spec,
      aUserContextId,
      false
    );
  }
}

/**
 * Helper for tests that generate search results but aren't interested in
 * suggestions, such as autofill tests. Installs a test engine and disables
 * suggestions.
 */
function testEngine_setup() {
  add_task(async function setup() {
    await cleanupPlaces();
    let engine = await addTestSuggestionsEngine();
    let oldDefaultEngine = await Services.search.getDefault();

    registerCleanupFunction(async () => {
      Services.prefs.clearUserPref("browser.urlbar.suggest.searches");
      Services.prefs.clearUserPref(
        "browser.search.separatePrivateDefault.ui.enabled"
      );
      Services.search.setDefault(oldDefaultEngine);
    });

    Services.search.setDefault(engine);
    Services.prefs.setBoolPref(
      "browser.search.separatePrivateDefault.ui.enabled",
      false
    );
    Services.prefs.setBoolPref("browser.urlbar.suggest.searches", false);
  });
}

async function cleanupPlaces() {
  Services.prefs.clearUserPref("browser.urlbar.autoFill");
  Services.prefs.clearUserPref("browser.urlbar.autoFill.searchEngines");

  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesUtils.history.clear();
}

/**
 * Returns the frecency of a url.
 *
 * @param {string} aURI The URI or spec to get frecency for.
 * @returns {number} the frecency value.
 */
function frecencyForUrl(aURI) {
  let url = aURI;
  if (aURI instanceof Ci.nsIURI) {
    url = aURI.spec;
  } else if (aURI instanceof URL) {
    url = aURI.href;
  }
  let stmt = DBConn().createStatement(
    "SELECT frecency FROM moz_places WHERE url_hash = hash(?1) AND url = ?1"
  );
  stmt.bindByIndex(0, url);
  try {
    if (!stmt.executeStep()) {
      throw new Error("No result for frecency.");
    }
    return stmt.getInt32(0);
  } finally {
    stmt.finalize();
  }
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
  {
    title,
    uri,
    iconUri,
    tags = [],
    heuristic = false,
    source = UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
  }
) {
  let result = new UrlbarResult(
    UrlbarUtils.RESULT_TYPE.URL,
    source,
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
 * Creates a UrlbarResult for a form history result.
 * @param {UrlbarQueryContext} queryContext
 *   The context that this result will be displayed in.
 * @param {string} options.suggestion
 *   The form history suggestion.
 * @param {string} options.engineName
 *   The name of the engine that will do the search when the result is picked.
 * @returns {UrlbarResult}
 */
function makeFormHistoryResult(queryContext, { suggestion, engineName }) {
  return new UrlbarResult(
    UrlbarUtils.RESULT_TYPE.SEARCH,
    UrlbarUtils.RESULT_SOURCE.HISTORY,
    ...UrlbarResult.payloadAndSimpleHighlights(queryContext.tokens, {
      engine: engineName,
      suggestion: [suggestion, UrlbarUtils.HIGHLIGHT.SUGGESTED],
      lowerCaseSuggestion: suggestion.toLocaleLowerCase(),
    })
  );
}

/**
 * Creates a UrlbarResult for an omnibox extension result. For more information,
 * see the documentation for omnibox.SuggestResult:
 * https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/omnibox/SuggestResult
 * @param {UrlbarQueryContext} queryContext
 *   The context that this result will be displayed in.
 * @param {string} options.content
 *   The string displayed when the result is highlighted.
 * @param {string} options.description
 *   The string displayed in the address bar dropdown.
 * @param {string} options.keyword
 *   The keyword associated with the extension returning the result.
 * @param {boolean} [options.heuristic]
 *   True if this is a heuristic result. Defaults to false.
 * @returns {UrlbarResult}
 */
function makeOmniboxResult(
  queryContext,
  { content, description, keyword, heuristic = false }
) {
  let result = new UrlbarResult(
    UrlbarUtils.RESULT_TYPE.OMNIBOX,
    UrlbarUtils.RESULT_SOURCE.OTHER_NETWORK,
    ...UrlbarResult.payloadAndSimpleHighlights(queryContext.tokens, {
      title: [description, UrlbarUtils.HIGHLIGHT.TYPED],
      content: [content, UrlbarUtils.HIGHLIGHT.TYPED],
      keyword: [keyword, UrlbarUtils.HIGHLIGHT.TYPED],
      icon: [UrlbarUtils.ICON.EXTENSION],
    })
  );
  result.heuristic = heuristic;
  return result;
}

/**
 * Creates a UrlbarResult for an switch-to-tab result.
 * @param {UrlbarQueryContext} queryContext
 *   The context that this result will be displayed in.
 * @param {string} options.uri
 *   The page URI.
 * @param {string} [options.title]
 *   The page title.
 * @param {string} [options.iconUri]
 *   A URI for the page icon.
 * @returns {UrlbarResult}
 */
function makeTabSwitchResult(queryContext, { uri, title, iconUri }) {
  return new UrlbarResult(
    UrlbarUtils.RESULT_TYPE.TAB_SWITCH,
    UrlbarUtils.RESULT_SOURCE.TABS,
    ...UrlbarResult.payloadAndSimpleHighlights(queryContext.tokens, {
      url: [uri, UrlbarUtils.HIGHLIGHT.TYPED],
      title: [title, UrlbarUtils.HIGHLIGHT.TYPED],
      // Check against undefined so consumers can pass in the empty string.
      icon: typeof iconUri != "undefined" ? iconUri : `page-icon:${uri}`,
    })
  );
}

/**
 * Creates a UrlbarResult for a keyword search result.
 * @param {UrlbarQueryContext} queryContext
 *   The context that this result will be displayed in.
 * @param {string} options.uri
 *   The page URI.
 * @param {string} options.keyword
 *   The page's search keyword.
 * @param {string} [options.title]
 *   The title for the bookmarked keyword page.
 * @param {string} [options.iconUri]
 *   A URI for the engine's icon.
 * @param {string} [options.postData]
 *   The search POST data.
 * @param {boolean} [options.heuristic]
 *   True if this is a heuristic result. Defaults to false.
 * @returns {UrlbarResult}
 */
function makeKeywordSearchResult(
  queryContext,
  { uri, keyword, title, iconUri, postData, heuristic = false }
) {
  let result = new UrlbarResult(
    UrlbarUtils.RESULT_TYPE.KEYWORD,
    UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
    ...UrlbarResult.payloadAndSimpleHighlights(queryContext.tokens, {
      title: [title ? title : uri, UrlbarUtils.HIGHLIGHT.TYPED],
      url: [uri, UrlbarUtils.HIGHLIGHT.TYPED],
      keyword: [keyword, UrlbarUtils.HIGHLIGHT.TYPED],
      input: [queryContext.searchString, UrlbarUtils.HIGHLIGHT.TYPED],
      postData: postData || null,
      icon: typeof iconUri != "undefined" ? iconUri : `page-icon:${uri}`,
    })
  );

  if (heuristic) {
    result.heuristic = heuristic;
  }
  return result;
}

/**
 * Creates a UrlbarResult for a priority search result.
 * @param {UrlbarQueryContext} queryContext
 *   The context that this result will be displayed in.
 * @param {string} [options.engineName]
 *   The name of the engine providing the suggestion. Leave blank if there
 *   is no suggestion.
 * @param {string} [options.engineIconUri]
 *   A URI for the engine's icon.
 * @param {boolean} [options.heuristic]
 *   True if this is a heuristic result. Defaults to false.
 * @returns {UrlbarResult}
 */
function makePrioritySearchResult(
  queryContext,
  { engineName, engineIconUri, heuristic }
) {
  let result = new UrlbarResult(
    UrlbarUtils.RESULT_TYPE.SEARCH,
    UrlbarUtils.RESULT_SOURCE.SEARCH,
    ...UrlbarResult.payloadAndSimpleHighlights(queryContext.tokens, {
      engine: [engineName, UrlbarUtils.HIGHLIGHT.TYPED],
      icon: engineIconUri,
    })
  );

  if (heuristic) {
    result.heuristic = heuristic;
  }
  return result;
}

/**
 * Creates a UrlbarResult for a remote tab result.
 * @param {UrlbarQueryContext} queryContext
 *   The context that this result will be displayed in.
 * @param {string} options.uri
 *   The page URI.
 * @param {string} options.device
 *   The name of the device that the remote tab comes from.
 * @param {string} [options.title]
 *   The page title.
 * @param {number} [options.lastUsed]
 *   The last time the remote tab was visited, in epoch seconds. Defaults
 *   to 0.
 * @param {string} [options.iconUri]
 *   A URI for the page's icon.
 * @returns {UrlbarResult}
 */
function makeRemoteTabResult(
  queryContext,
  { uri, device, title, iconUri, lastUsed = 0 }
) {
  let payload = {
    url: [uri, UrlbarUtils.HIGHLIGHT.TYPED],
    device: [device, UrlbarUtils.HIGHLIGHT.TYPED],
    // Check against undefined so consumers can pass in the empty string.
    icon:
      typeof iconUri != "undefined"
        ? iconUri
        : `moz-anno:favicon:page-icon:${uri}`,
    lastUsed: lastUsed * 1000,
  };

  // Check against undefined so consumers can pass in the empty string.
  if (typeof title != "undefined") {
    payload.title = [title, UrlbarUtils.HIGHLIGHT.TYPED];
  } else {
    payload.title = [uri, UrlbarUtils.HIGHLIGHT.TYPED];
  }

  let result = new UrlbarResult(
    UrlbarUtils.RESULT_TYPE.REMOTE_TAB,
    UrlbarUtils.RESULT_SOURCE.TABS,
    ...UrlbarResult.payloadAndSimpleHighlights(queryContext.tokens, payload)
  );

  return result;
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
 * @param {string} [options.uri]
 *   The URI that the search result will navigate to.
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
 * @param {boolean} [options.providesSearchMode]
 *   Whether search mode is entered when this result is selected.
 * @param {string} [options.providerName]
 *   The name of the provider offering this result. The test suite will not
 *   check which provider offered a result unless this option is specified.
 * @returns {UrlbarResult}
 */
function makeSearchResult(
  queryContext,
  {
    suggestion,
    tailPrefix,
    tail,
    tailOffsetIndex,
    engineName,
    alias,
    uri,
    query,
    engineIconUri,
    providesSearchMode,
    providerName,
    inPrivateWindow,
    isPrivateEngine,
    heuristic = false,
    type = UrlbarUtils.RESULT_TYPE.SEARCH,
    source = UrlbarUtils.RESULT_SOURCE.SEARCH,
    satisfiesAutofillThreshold = false,
  }
) {
  // Tail suggestion common cases, handled here to reduce verbosity in tests.
  if (tail) {
    if (!tailPrefix) {
      tailPrefix = "… ";
    }
    if (!tailOffsetIndex) {
      tailOffsetIndex = suggestion.indexOf(tail);
    }
  }

  let payload = {
    engine: [engineName, UrlbarUtils.HIGHLIGHT.TYPED],
    suggestion: [suggestion, UrlbarUtils.HIGHLIGHT.SUGGESTED],
    tailPrefix,
    tail: [tail, UrlbarUtils.HIGHLIGHT.SUGGESTED],
    tailOffsetIndex,
    keyword: [
      alias,
      providesSearchMode
        ? UrlbarUtils.HIGHLIGHT.TYPED
        : UrlbarUtils.HIGHLIGHT.NONE,
    ],
    // Check against undefined so consumers can pass in the empty string.
    query: [
      typeof query != "undefined" ? query : queryContext.trimmedSearchString,
      UrlbarUtils.HIGHLIGHT.TYPED,
    ],
    icon: engineIconUri,
    providesSearchMode,
    inPrivateWindow,
    isPrivateEngine,
  };

  // Passing even an undefined URL in the payload creates a potentially-unwanted
  // displayUrl parameter, so we add it only if specified.
  if (uri) {
    payload.url = uri;
  }
  if (providerName == "TabToSearch") {
    payload.satisfiesAutofillThreshold = satisfiesAutofillThreshold;
    if (payload.url.startsWith("www.")) {
      payload.url = payload.url.substring(4);
    }
    payload.isGeneralPurposeEngine = false;
  }

  let result = new UrlbarResult(
    type,
    source,
    ...UrlbarResult.payloadAndSimpleHighlights(queryContext.tokens, payload)
  );

  if (typeof suggestion == "string") {
    result.payload.lowerCaseSuggestion = result.payload.suggestion.toLocaleLowerCase();
  }

  if (providerName) {
    result.providerName = providerName;
  }

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
 *  * @param {string} providerName
 *   The name of the provider offering this result. The test suite will not
 *   check which provider offered a result unless this option is specified.
 * @returns {UrlbarResult}
 */
function makeVisitResult(
  queryContext,
  {
    title,
    uri,
    iconUri,
    providerName,
    tags = [],
    heuristic = false,
    source = UrlbarUtils.RESULT_SOURCE.HISTORY,
  }
) {
  let payload = {
    url: [uri, UrlbarUtils.HIGHLIGHT.TYPED],
    title: [title, UrlbarUtils.HIGHLIGHT.TYPED],
  };

  if (iconUri) {
    payload.icon = iconUri;
  } else if (
    iconUri === undefined &&
    source != UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL
  ) {
    payload.icon = `page-icon:${uri}`;
  }

  if (!heuristic && tags) {
    payload.tags = [tags, UrlbarUtils.HIGHLIGHT.TYPED];
  }

  let result = new UrlbarResult(
    UrlbarUtils.RESULT_TYPE.URL,
    source,
    ...UrlbarResult.payloadAndSimpleHighlights(queryContext.tokens, payload)
  );

  if (providerName) {
    result.providerName = providerName;
  }

  result.heuristic = heuristic;
  return result;
}

/**
 * Checks that the results returned by a UrlbarController match those in
 * the param `matches`.
 * @param {UrlbarQueryContext} context
 *   The context for this query.
 * @param {string} [incompleteSearch]
 *   A search will be fired for this string and then be immediately canceled by
 *   the query in `context`.
 * @param {string} [autofilled]
 *   The autofilled value in the first result.
 * @param {string} [completed]
 *   The value that would be filled if the autofill result was confirmed.
 *   Has no effect if `autofilled` is not specified.
 * @param {array} matches
 *   An array of UrlbarResults.
 * @param {boolean} [isPrivate]
 *   Set this to `true` to simulate a search in a private window.
 */
async function check_results({
  context,
  incompleteSearch,
  autofilled,
  completed,
  matches = [],
} = {}) {
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
      onFirstResult() {
        return false;
      },
      window: {
        location: {
          href: AppConstants.BROWSER_CHROME_URL,
        },
      },
    },
  });

  if (incompleteSearch) {
    let incompleteContext = createContext(incompleteSearch, {
      isPrivate: context.isPrivate,
    });
    controller.startQuery(incompleteContext);
  }
  await controller.startQuery(context);

  if (autofilled) {
    Assert.ok(context.results[0], "There is a first result.");
    Assert.ok(
      context.results[0].autofill,
      "The first result is an autofill result"
    );
    Assert.equal(
      context.results[0].autofill.value,
      autofilled,
      "The correct value was autofilled."
    );
    if (completed) {
      Assert.equal(
        context.results[0].payload.url,
        completed,
        "The completed autofill value is correct."
      );
    }
  }
  if (context.results.length != matches.length) {
    info("Actual results: " + JSON.stringify(context.results));
  }
  Assert.equal(
    context.results.length,
    matches.length,
    "Found the expected number of results."
  );

  function getPayload(result) {
    let payload = {};
    for (let [key, value] of Object.entries(result.payload)) {
      if (value !== undefined) {
        payload[key] = value;
      }
    }
    return payload;
  }

  for (let i = 0; i < matches.length; i++) {
    let actual = context.results[i];
    let expected = matches[i];
    info(
      `Comparing results at index ${i}:` +
        " actual=" +
        JSON.stringify(actual) +
        " expected=" +
        JSON.stringify(expected)
    );
    Assert.equal(
      actual.type,
      expected.type,
      `result.type at result index ${i}`
    );
    Assert.equal(
      actual.source,
      expected.source,
      `result.source at result index ${i}`
    );
    Assert.equal(
      actual.heuristic,
      expected.heuristic,
      `result.heuristic at result index ${i}`
    );
    if (expected.providerName) {
      Assert.equal(
        actual.providerName,
        expected.providerName,
        `result.providerName at result index ${i}`
      );
    }
    Assert.deepEqual(
      getPayload(actual),
      getPayload(expected),
      `result.payload at result index ${i}`
    );
  }
}

/**
 * Returns the frecency of an origin.
 *
 * @param   {string} prefix
 *          The origin's prefix, e.g., "http://".
 * @param   {string} aHost
 *          The origin's host.
 * @returns {number} The origin's frecency.
 */
async function getOriginFrecency(prefix, aHost) {
  let db = await PlacesUtils.promiseDBConnection();
  let rows = await db.execute(
    `
    SELECT frecency
    FROM moz_origins
    WHERE prefix = :prefix AND host = :host
  `,
    { prefix, host: aHost }
  );
  Assert.equal(rows.length, 1);
  return rows[0].getResultByIndex(0);
}

/**
 * Returns the origin frecency stats.
 *
 * @returns {object}
 *          An object { count, sum, squares }.
 */
async function getOriginFrecencyStats() {
  let db = await PlacesUtils.promiseDBConnection();
  let rows = await db.execute(`
    SELECT
      IFNULL((SELECT value FROM moz_meta WHERE key = 'origin_frecency_count'), 0),
      IFNULL((SELECT value FROM moz_meta WHERE key = 'origin_frecency_sum'), 0),
      IFNULL((SELECT value FROM moz_meta WHERE key = 'origin_frecency_sum_of_squares'), 0)
  `);
  let count = rows[0].getResultByIndex(0);
  let sum = rows[0].getResultByIndex(1);
  let squares = rows[0].getResultByIndex(2);
  return { count, sum, squares };
}

/**
 * Returns the origin autofill frecency threshold.
 *
 * @returns {number}
 *          The threshold.
 */
async function getOriginAutofillThreshold() {
  let { count, sum, squares } = await getOriginFrecencyStats();
  if (!count) {
    return 0;
  }
  if (count == 1) {
    return sum;
  }
  let stddevMultiplier = UrlbarPrefs.get("autoFill.stddevMultiplier");
  return (
    sum / count +
    stddevMultiplier * Math.sqrt((squares - (sum * sum) / count) / count)
  );
}

/**
 * Checks that origins appear in a given order in the database.
 * @param {string} host The "fixed" host, without "www."
 * @param {Array} prefixOrder The prefixes (scheme + www.) sorted appropriately.
 */
async function checkOriginsOrder(host, prefixOrder) {
  await PlacesUtils.withConnectionWrapper("checkOriginsOrder", async db => {
    let prefixes = (
      await db.execute(
        `SELECT prefix || iif(instr(host, "www.") = 1, "www.", "")
         FROM moz_origins
         WHERE host = :host OR host = "www." || :host
         ORDER BY ROWID ASC
        `,
        { host }
      )
    ).map(r => r.getResultByIndex(0));
    Assert.deepEqual(prefixes, prefixOrder);
  });
}
