/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

var {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");

// Import common head.
/* import-globals-from ../../../../../toolkit/components/places/tests/head_common.js */
var commonFile = do_get_file("../../../../../toolkit/components/places/tests/head_common.js", false);
if (commonFile) {
  let uri = Services.io.newFileURI(commonFile);
  Services.scriptloader.loadSubScript(uri.spec, this);
}

// Put any other stuff relative to this test folder below.
var {UrlbarMuxer, UrlbarProvider, UrlbarQueryContext, UrlbarUtils} = ChromeUtils.import("resource:///modules/UrlbarUtils.jsm");
XPCOMUtils.defineLazyModuleGetters(this, {
  HttpServer: "resource://testing-common/httpd.js",
  PlacesTestUtils: "resource://testing-common/PlacesTestUtils.jsm",
  PlacesUtils: "resource://gre/modules/PlacesUtils.jsm",
  UrlbarController: "resource:///modules/UrlbarController.jsm",
  UrlbarInput: "resource:///modules/UrlbarInput.jsm",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.jsm",
  UrlbarProviderOpenTabs: "resource:///modules/UrlbarProviderOpenTabs.jsm",
  UrlbarProvidersManager: "resource:///modules/UrlbarProvidersManager.jsm",
  UrlbarResult: "resource:///modules/UrlbarResult.jsm",
  UrlbarTokenizer: "resource:///modules/UrlbarTokenizer.jsm",
});
const {sinon} = ChromeUtils.import("resource://testing-common/Sinon.jsm");

/**
 * @param {string} searchString The search string to insert into the context.
 * @param {object} properties Overrides for the default values.
 * @returns {UrlbarQueryContext} Creates a dummy query context with pre-filled
 *          required options.
 */
function createContext(searchString = "foo", properties = {}) {
  let context = new UrlbarQueryContext({
    allowAutofill: UrlbarPrefs.get("autoFill"),
    isPrivate: true,
    maxResults: UrlbarPrefs.get("maxRichResults"),
    searchString,
  });
  return Object.assign(context, properties);
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
function promiseControllerNotification(controller, notification, expected = true) {
  return new Promise((resolve, reject) => {
    let proxifiedObserver = new Proxy({}, {
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
    });
    controller.addQueryListener(proxifiedObserver);
  });
}

/**
 * A basic test provider, returning all the provided matches.
 */
class TestProvider extends UrlbarProvider {
  constructor(matches, cancelCallback, type = UrlbarUtils.PROVIDER_TYPE.PROFILE) {
    super();
    this._name = "TestProvider" + Math.floor(Math.random() * 100000);
    this._cancelCallback = cancelCallback;
    this._matches = matches;
    this._type = type;
  }
  get name() {
    return this._name;
  }
  get type() {
    return this._type;
  }
  isActive(context) {
    Assert.ok(context, "context is passed-in");
    return true;
  }
  isRestricting(context) {
    Assert.ok(context, "context is passed-in");
    return false;
  }
  async startQuery(context, add) {
    Assert.ok(context, "context is passed-in");
    Assert.equal(typeof add, "function", "add is a callback");
    this._context = context;
    for (const match of this._matches) {
      add(this, match);
    }
  }
  cancelQuery(context) {
    // If the query was created but didn't run, this_context will be undefined.
    if (this._context) {
      Assert.equal(this._context, context, "cancelQuery: context is the same");
    }
    if (this._cancelCallback) {
      this._cancelCallback();
    }
  }
}

/**
 * Helper function to clear the existing providers and register a basic provider
 * that returns only the results given.
 *
 * @param {array} matches The matches for the provider to return.
 * @param {function} [cancelCallback] Optional, called when the query provider
 *                                    receives a cancel instruction.
 * @param {UrlbarUtils.PROVIDER_TYPE} type The provider type.
 * @returns {string} name of the registered provider
 */
function registerBasicTestProvider(matches = [], cancelCallback, type) {
  let provider = new TestProvider(matches, cancelCallback, type);
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
function addTestSuggestionsEngine(suggestionsFn = null) {
  // This port number should match the number in engine-suggestions.xml.
  let server = makeTestServer(9000);
  server.registerPathHandler("/suggest", (req, resp) => {
    // URL query params are x-www-form-urlencoded, which converts spaces into
    // plus signs, so un-convert any plus signs back to spaces.
    let searchStr = decodeURIComponent(req.queryString.replace(/\+/g, " "));
    let suggestions =
      suggestionsFn ? suggestionsFn(searchStr) :
      [searchStr].concat(["foo", "bar"].map(s => searchStr + " " + s));
    let data = [searchStr, suggestions];
    resp.setHeader("Content-Type", "application/json", false);
    resp.write(JSON.stringify(data));
  });
  return addTestEngine("engine-suggestions.xml", server);
}
