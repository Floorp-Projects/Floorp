"use strict";

const TOTAL_URI_COUNT = "browser.engagement.total_uri_count";
const UNIQUE_DOMAINS_COUNT = "browser.engagement.unique_domains_count";
const UNFILTERED_URI_COUNT = "browser.engagement.unfiltered_uri_count";
const TELEMETRY_SUBSESSION_TOPIC = "internal-telemetry-after-subsession-split";

// Reset internal URI counter in case URIs were opened by other tests.
Services.obs.notifyObservers(null, TELEMETRY_SUBSESSION_TOPIC);

/**
 * Waits for the web progress listener associated with this tab to fire an
 * onLocationChange for a non-error page.
 *
 * @param {xul:browser} browser
 *        A xul:browser.
 *
 * @return {Promise}
 * @resolves When navigating to a non-error page.
 */
function browserLocationChanged(browser) {
  return new Promise(resolve => {
    let wpl = {
      onStateChange() {},
      onSecurityChange() {},
      onStatusChange() {},
      onLocationChange(aWebProgress, aRequest, aURI, aFlags) {
        if (!(aFlags & Ci.nsIWebProgressListener.LOCATION_CHANGE_ERROR_PAGE)) {
          browser.webProgress.removeProgressListener(filter);
          filter.removeProgressListener(wpl);
          resolve();
        }
      },
      QueryInterface: ChromeUtils.generateQI([
        Ci.nsIWebProgressListener,
        Ci.nsIWebProgressListener2,
      ]),
    };
    const filter = Cc["@mozilla.org/appshell/component/browser-status-filter;1"]
                     .createInstance(Ci.nsIWebProgress);
    filter.addProgressListener(wpl, Ci.nsIWebProgress.NOTIFY_ALL);
    browser.webProgress.addProgressListener(filter, Ci.nsIWebProgress.NOTIFY_ALL);
  });
}

add_task(async function test_URIAndDomainCounts() {
  // Let's reset the counts.
  Services.telemetry.clearScalars();

  let checkCounts = (countsObject) => {
    // Get a snapshot of the scalars and then clear them.
    const scalars = getParentProcessScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN);
    checkScalar(scalars, TOTAL_URI_COUNT, countsObject.totalURIs,
                "The URI scalar must contain the expected value.");
    checkScalar(scalars, UNIQUE_DOMAINS_COUNT, countsObject.domainCount,
                "The unique domains scalar must contain the expected value.");
    checkScalar(scalars, UNFILTERED_URI_COUNT, countsObject.totalUnfilteredURIs,
                "The unfiltered URI scalar must contain the expected value.");
  };

  // Check that about:blank doesn't get counted in the URI total.
  let firstTab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank");
  checkCounts({totalURIs: 0, domainCount: 0, totalUnfilteredURIs: 0});

  // Open a different page and check the counts.
  await BrowserTestUtils.loadURI(firstTab.linkedBrowser, "http://example.com/");
  await BrowserTestUtils.browserLoaded(firstTab.linkedBrowser);
  checkCounts({totalURIs: 1, domainCount: 1, totalUnfilteredURIs: 1});

  // Activating a different tab must not increase the URI count.
  let secondTab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank");
  await BrowserTestUtils.switchTab(gBrowser, firstTab);
  checkCounts({totalURIs: 1, domainCount: 1, totalUnfilteredURIs: 1});
  BrowserTestUtils.removeTab(secondTab);

  // Open a new window and set the tab to a new address.
  let newWin = await BrowserTestUtils.openNewBrowserWindow();
  await BrowserTestUtils.loadURI(newWin.gBrowser.selectedBrowser, "http://example.com/");
  await BrowserTestUtils.browserLoaded(newWin.gBrowser.selectedBrowser);
  checkCounts({totalURIs: 2, domainCount: 1, totalUnfilteredURIs: 2});

  // We should not count AJAX requests.
  const XHR_URL = "http://example.com/r";
  await ContentTask.spawn(newWin.gBrowser.selectedBrowser, XHR_URL, function(url) {
    return new Promise(resolve => {
      var xhr = new content.window.XMLHttpRequest();
      xhr.open("GET", url);
      xhr.onload = () => resolve();
      xhr.send();
    });
  });
  checkCounts({totalURIs: 2, domainCount: 1, totalUnfilteredURIs: 2});

  // Check that we're counting page fragments.
  let loadingStopped = browserLocationChanged(newWin.gBrowser.selectedBrowser);
  await BrowserTestUtils.loadURI(newWin.gBrowser.selectedBrowser, "http://example.com/#2");
  await loadingStopped;
  checkCounts({totalURIs: 3, domainCount: 1, totalUnfilteredURIs: 3});

  // Check that a different URI from the example.com domain doesn't increment the unique count.
  await BrowserTestUtils.loadURI(newWin.gBrowser.selectedBrowser, "http://test1.example.com/");
  await BrowserTestUtils.browserLoaded(newWin.gBrowser.selectedBrowser);
  checkCounts({totalURIs: 4, domainCount: 1, totalUnfilteredURIs: 4});

  // Make sure that the unique domains counter is incrementing for a different domain.
  await BrowserTestUtils.loadURI(newWin.gBrowser.selectedBrowser, "https://example.org/");
  await BrowserTestUtils.browserLoaded(newWin.gBrowser.selectedBrowser);
  checkCounts({totalURIs: 5, domainCount: 2, totalUnfilteredURIs: 5});

  // Check that we only account for top level loads (e.g. we don't count URIs from
  // embedded iframes).
  await ContentTask.spawn(newWin.gBrowser.selectedBrowser, null, async function() {
    let doc = content.document;
    let iframe = doc.createElement("iframe");
    let promiseIframeLoaded = ContentTaskUtils.waitForEvent(iframe, "load", false);
    iframe.src = "https://example.org/test";
    doc.body.insertBefore(iframe, doc.body.firstChild);
    await promiseIframeLoaded;
  });
  checkCounts({totalURIs: 5, domainCount: 2, totalUnfilteredURIs: 5});

  // Check that uncommon protocols get counted in the unfiltered URI probe.
  const TEST_PAGE =
    "data:text/html,<a id='target' href='%23par1'>Click me</a><a name='par1'>The paragraph.</a>";
  await BrowserTestUtils.loadURI(newWin.gBrowser.selectedBrowser, TEST_PAGE);
  await BrowserTestUtils.browserLoaded(newWin.gBrowser.selectedBrowser);
  checkCounts({totalURIs: 5, domainCount: 2, totalUnfilteredURIs: 6});

  // Clean up.
  BrowserTestUtils.removeTab(firstTab);
  await BrowserTestUtils.closeWindow(newWin);
});
