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

ChromeUtils.import("resource:///modules/UrlbarController.jsm");
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
  UrlbarUtils: "resource:///modules/UrlbarUtils.jsm",
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
 * @returns {QueryContext} Creates a dummy query context with pre-filled required options.
 */
function createContext(searchString = "foo") {
  return new QueryContext({
    searchString,
    lastKey: searchString ? searchString[searchString.length - 1] : "",
    maxResults: Services.prefs.getIntPref("browser.urlbar.maxRichResults"),
    isPrivate: true,
  });
}

/**
 * Waits for the given notification from the supplied controller.
 *
 * @param {UrlbarController} controller The controller to wait for a response from.
 * @param {string} notification The name of the notification to wait for.
 * @returns {Promise} A promise that is resolved with the arguments supplied to
 *   the notification.
 */
function promiseControllerNotification(controller, notification) {
  return new Promise(resolve => {
    let proxifiedObserver = new Proxy({}, {
      get: (target, name) => {
        if (name == notification) {
          return (...args) => {
            controller.removeQueryListener(proxifiedObserver);
            resolve(args);
          };
        }
        return () => false;
      },
    });
    controller.addQueryListener(proxifiedObserver);
  });
}
