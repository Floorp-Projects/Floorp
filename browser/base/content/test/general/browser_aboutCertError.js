/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// This is testing the aboutCertError page (Bug 1207107).  It's a start,
// but should be expanded to include cert_domain_link / badStsCert

const GOOD_PAGE = "https://example.com/";
const BAD_CERT = "https://expired.example.com/";

add_task(function* checkReturnToAboutHome() {
  info("Loading a bad cert page directly and making sure 'return to previous page' goes to about:home");
  let browser;
  let certErrorLoaded;
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, () => {
    gBrowser.selectedTab = gBrowser.addTab(BAD_CERT);
    browser = gBrowser.selectedBrowser;
    certErrorLoaded = waitForCertErrorLoad(browser);
  }, false);

  info("Loading and waiting for the cert error");
  yield certErrorLoaded;

  is(browser.webNavigation.canGoBack, false, "!webNavigation.canGoBack");
  is(browser.webNavigation.canGoForward, false, "!webNavigation.canGoForward");

  info("Clicking the go back button on about:certerror");
  let pageshowPromise = promiseWaitForEvent(browser, "pageshow");
  yield ContentTask.spawn(browser, null, function* () {
    let doc = content.document;
    let returnButton = doc.getElementById("returnButton");
    returnButton.click();
  });
  yield pageshowPromise;

  is(browser.webNavigation.canGoBack, true, "webNavigation.canGoBack");
  is(browser.webNavigation.canGoForward, false, "!webNavigation.canGoForward");
  is(gBrowser.currentURI.spec, "about:home", "Went back");

  gBrowser.removeCurrentTab();
});

add_task(function* checkReturnToPreviousPage() {
  info("Loading a bad cert page and making sure 'return to previous page' goes back");
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, GOOD_PAGE);
  let browser = gBrowser.selectedBrowser;

  info("Loading and waiting for the cert error");
  let certErrorLoaded = waitForCertErrorLoad(browser);
  BrowserTestUtils.loadURI(browser, BAD_CERT);
  yield certErrorLoaded;

  is(browser.webNavigation.canGoBack, true, "webNavigation.canGoBack");
  is(browser.webNavigation.canGoForward, false, "!webNavigation.canGoForward");

  info("Clicking the go back button on about:certerror");
  let pageshowPromise = promiseWaitForEvent(browser, "pageshow");
  yield ContentTask.spawn(browser, null, function* () {
    let doc = content.document;
    let returnButton = doc.getElementById("returnButton");
    returnButton.click();
  });
  yield pageshowPromise;

  is(browser.webNavigation.canGoBack, false, "!webNavigation.canGoBack");
  is(browser.webNavigation.canGoForward, true, "webNavigation.canGoForward");
  is(gBrowser.currentURI.spec, GOOD_PAGE, "Went back");

  gBrowser.removeCurrentTab();
});

function waitForCertErrorLoad(browser) {
  return new Promise(resolve => {
    info("Waiting for DOMContentLoaded event");
    browser.addEventListener("DOMContentLoaded", function load() {
      browser.removeEventListener("DOMContentLoaded", load, false, true);
      resolve();
    }, false, true);
  });
}
