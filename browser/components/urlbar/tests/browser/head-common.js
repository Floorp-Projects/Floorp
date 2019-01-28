/* eslint-env mozilla/frame-script */

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  HttpServer: "resource://testing-common/httpd.js",
  PlacesUtils: "resource://gre/modules/PlacesUtils.jsm",
  PlacesTestUtils: "resource://testing-common/PlacesTestUtils.jsm",
  Preferences: "resource://gre/modules/Preferences.jsm",
  SearchTestUtils: "resource://testing-common/SearchTestUtils.jsm",
  UrlbarTestUtils: "resource://testing-common/UrlbarTestUtils.jsm",
  UrlbarTokenizer: "resource:///modules/UrlbarTokenizer.jsm",
});

SearchTestUtils.init(Assert, registerCleanupFunction);

function is_element_visible(element, msg) {
  isnot(element, null, "Element should not be null, when checking visibility");
  ok(BrowserTestUtils.is_visible(element), msg || "Element should be visible");
}

function is_element_hidden(element, msg) {
  isnot(element, null, "Element should not be null, when checking visibility");
  ok(BrowserTestUtils.is_hidden(element), msg || "Element should be hidden");
}

function runHttpServer(scheme, host, port = -1) {
  let httpserver = new HttpServer();
  try {
    httpserver.start(port);
    port = httpserver.identity.primaryPort;
    httpserver.identity.setPrimary(scheme, host, port);
  } catch (ex) {
    info("We can't launch our http server successfully.");
  }
  is(httpserver.identity.has(scheme, host, port), true, `${scheme}://${host}:${port} is listening.`);
  return httpserver;
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
  return UrlbarTestUtils.promiseAutocompleteResultPopup(inputText,
    win, waitForFocus, fireInputEvent);
}

function promiseSpeculativeConnection(httpserver) {
  return UrlbarTestUtils.promiseSpeculativeConnection(httpserver);
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
