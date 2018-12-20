/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

ChromeUtils.import("resource://gre/modules/Services.jsm");

// Import common head.
/* import-globals-from ../../../../../toolkit/components/places/tests/head_common.js */
var commonFile = do_get_file("../../../../../toolkit/components/places/tests/head_common.js", false);
if (commonFile) {
  let uri = Services.io.newFileURI(commonFile);
  Services.scriptloader.loadSubScript(uri.spec, this);
}

// Put any other stuff relative to this test folder below.
ChromeUtils.import("resource:///modules/UrlbarUtils.jsm");
XPCOMUtils.defineLazyModuleGetters(this, {
  PlacesTestUtils: "resource://testing-common/PlacesTestUtils.jsm",
  PlacesUtils: "resource://gre/modules/PlacesUtils.jsm",
  UrlbarController: "resource:///modules/UrlbarController.jsm",
  UrlbarInput: "resource:///modules/UrlbarInput.jsm",
  UrlbarMatch: "resource:///modules/UrlbarMatch.jsm",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.jsm",
  UrlbarProviderOpenTabs: "resource:///modules/UrlbarProviderOpenTabs.jsm",
  UrlbarProvidersManager: "resource:///modules/UrlbarProvidersManager.jsm",
  UrlbarTokenizer: "resource:///modules/UrlbarTokenizer.jsm",
});

// ================================================
// Load mocking/stubbing library, sinon
// docs: http://sinonjs.org/releases/v2.3.2/
// Sinon needs Timer.jsm for setTimeout etc.
ChromeUtils.import("resource://gre/modules/Timer.jsm");
Services.scriptloader.loadSubScript("resource://testing-common/sinon-2.3.2.js", this);
/* globals sinon */
// ================================================

/**
 * @param {string} searchString The search string to insert into the context.
 * @param {object} properties Overrides for the default values.
 * @returns {QueryContext} Creates a dummy query context with pre-filled required options.
 */
function createContext(searchString = "foo", properties = {}) {
  let context = new QueryContext({
    searchString,
    lastKey: searchString ? searchString[searchString.length - 1] : "",
    maxResults: UrlbarPrefs.get("maxRichResults"),
    isPrivate: true,
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
  constructor(matches, cancelCallback) {
    super();
    this._name = "TestProvider" + Math.floor(Math.random() * 100000);
    this._cancel = cancelCallback;
    this._matches = matches;
  }
  get name() {
    return this._name;
  }
  get type() {
    return UrlbarUtils.PROVIDER_TYPE.PROFILE;
  }
  get sources() {
    return this._matches.map(r => r.source);
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
    Assert.equal(this._context, context, "context is the same");
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
 * @returns {string} name of the registered provider
 */
function registerBasicTestProvider(matches, cancelCallback) {
  let provider = new TestProvider(matches, cancelCallback);
  UrlbarProvidersManager.registerProvider(provider);
  return provider.name;
}
