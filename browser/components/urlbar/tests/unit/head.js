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
ChromeUtils.defineModuleGetter(this, "UrlbarTokenizer",
                               "resource:///modules/UrlbarTokenizer.jsm");
ChromeUtils.defineModuleGetter(this, "PlacesTestUtils",
                               "resource://testing-common/PlacesTestUtils.jsm");

/**
 * @param {string} searchString The search string to insert into the context.
 * @returns {QueryContext} Creates a dummy query context with pre-filled required options.
 */
function createContext(searchString = "foo") {
  return new QueryContext({
    searchString,
    lastKey: searchString ? searchString[searchString.length - 1] : "",
    maxResults: 1,
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
