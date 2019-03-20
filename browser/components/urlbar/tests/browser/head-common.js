/* eslint-env mozilla/frame-script */

var {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  HttpServer: "resource://testing-common/httpd.js",
  PlacesUtils: "resource://gre/modules/PlacesUtils.jsm",
  PlacesTestUtils: "resource://testing-common/PlacesTestUtils.jsm",
  Preferences: "resource://gre/modules/Preferences.jsm",
  SearchTestUtils: "resource://testing-common/SearchTestUtils.jsm",
  UrlbarTestUtils: "resource://testing-common/UrlbarTestUtils.jsm",
  UrlbarTokenizer: "resource:///modules/UrlbarTokenizer.jsm",
});

XPCOMUtils.defineLazyGetter(this, "TEST_BASE_URL", () =>
  getRootDirectory(gTestPath).replace("chrome://mochitests/content",
                                      "https://example.com"));

SearchTestUtils.init(Assert, registerCleanupFunction);

function is_element_visible(element, msg) {
  isnot(element, null, "Element should not be null, when checking visibility");
  ok(BrowserTestUtils.is_visible(element), msg || "Element should be visible");
}

function is_element_hidden(element, msg) {
  isnot(element, null, "Element should not be null, when checking visibility");
  ok(BrowserTestUtils.is_hidden(element), msg || "Element should be hidden");
}

/**
 * Initializes an HTTP Server, and runs a task with it.
 * @param {object} details {scheme, host, port}
 * @param {function} taskFn The task to run, gets the server as argument.
 */
async function withHttpServer(details = { scheme: "http", host: "localhost", port: -1}, taskFn) {
  let server = new HttpServer();
  let url = `${details.scheme}://${details.host}:${details.port}`;
  try {
    info(`starting HTTP Server for ${url}`);
    try {
      server.start(details.port);
      details.port = server.identity.primaryPort;
      server.identity.setPrimary(details.scheme, details.host, details.port);
    } catch (ex) {
      throw new Error("We can't launch our http server successfully. " + ex);
    }
    Assert.ok(server.identity.has(details.scheme, details.host, details.port),
              `${url} is listening.`);
    try {
      await taskFn(server);
    } catch (ex) {
      throw new Error("Exception in the task function " + ex);
    }
  } finally {
    server.identity.remove(details.scheme, details.host, details.port);
    try {
      await new Promise(resolve => server.stop(resolve));
    } catch (ex) {}
    server = null;
  }
}

function promisePopupShown(popup) {
  return BrowserTestUtils.waitForPopupEvent(popup, "shown");
}

function promisePopupHidden(popup) {
  return BrowserTestUtils.waitForPopupEvent(popup, "hidden");
}

function promiseSearchComplete(win = window, dontAnimate = false) {
  return UrlbarTestUtils.promiseSearchComplete(win, dontAnimate);
}

function promiseAutocompleteResultPopup(inputText,
                                        win = window,
                                        fireInputEvent = false) {
  return UrlbarTestUtils.promiseAutocompleteResultPopup(win, inputText,
    waitForFocus, fireInputEvent);
}

async function waitForAutocompleteResultAt(index) {
  return UrlbarTestUtils.waitForAutocompleteResultAt(window, index);
}

function promiseSuggestionsPresent(msg = "") {
  return UrlbarTestUtils.promiseSuggestionsPresent(window, msg);
}

function suggestionsPresent() {
  return UrlbarTestUtils.suggestionsPresent(window);
}
