/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function () {
  const tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank"
  );
  const browser = tab.linkedBrowser;
  const aboutBlankBrowsingContext = browser.browsingContext;
  const { webProgress } = aboutBlankBrowsingContext;
  ok(
    webProgress,
    "Got a WebProgress interface on BrowsingContext in the parent process"
  );
  is(
    webProgress.browsingContext,
    browser.browsingContext,
    "WebProgress.browsingContext refers to the right BrowsingContext"
  );

  const onLocationChanged = waitForNextLocationChange(webProgress);
  const newLocation = "data:text/html;charset=utf-8,first-page";
  let loaded = BrowserTestUtils.browserLoaded(browser);
  BrowserTestUtils.startLoadingURIString(browser, newLocation);
  await loaded;

  const firstPageBrowsingContext = browser.browsingContext;
  const isBfcacheInParentEnabled =
    SpecialPowers.Services.appinfo.sessionHistoryInParent &&
    SpecialPowers.Services.prefs.getBoolPref("fission.bfcacheInParent");
  if (isBfcacheInParentEnabled) {
    isnot(
      aboutBlankBrowsingContext,
      firstPageBrowsingContext,
      "With bfcache in parent, navigations spawn a new BrowsingContext"
    );
  } else {
    is(
      aboutBlankBrowsingContext,
      firstPageBrowsingContext,
      "Without bfcache in parent, navigations reuse the same BrowsingContext"
    );
  }

  info("Wait for onLocationChange to be fired");
  {
    const { browsingContext, location, request, flags } =
      await onLocationChanged;
    is(
      browsingContext,
      firstPageBrowsingContext,
      "Location change fires on the new BrowsingContext"
    );
    ok(location instanceof Ci.nsIURI);
    is(location.spec, newLocation);
    ok(request instanceof Ci.nsIChannel);
    is(request.URI.spec, newLocation);
    is(flags, 0);
  }

  const onIframeLocationChanged = waitForNextLocationChange(webProgress);
  const iframeLocation = "data:text/html;charset=utf-8,iframe";
  const iframeBC = await SpecialPowers.spawn(
    browser,
    [iframeLocation],
    async url => {
      const iframe = content.document.createElement("iframe");
      await new Promise(resolve => {
        iframe.addEventListener("load", resolve, { once: true });
        iframe.src = url;
        content.document.body.appendChild(iframe);
      });

      return iframe.browsingContext;
    }
  );
  ok(
    iframeBC.webProgress,
    "The iframe BrowsingContext also exposes a WebProgress"
  );
  {
    const { browsingContext, location, request, flags } =
      await onIframeLocationChanged;
    is(
      browsingContext,
      iframeBC,
      "Iframe location change fires on the iframe BrowsingContext"
    );
    ok(location instanceof Ci.nsIURI);
    is(location.spec, iframeLocation);
    ok(request instanceof Ci.nsIChannel);
    is(request.URI.spec, iframeLocation);
    is(flags, 0);
  }

  const onSecondLocationChanged = waitForNextLocationChange(webProgress);
  const onSecondPageDocumentStart = waitForNextDocumentStart(webProgress);
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  const secondLocation = "http://example.com/document-builder.sjs?html=com";
  loaded = BrowserTestUtils.browserLoaded(browser);
  BrowserTestUtils.startLoadingURIString(browser, secondLocation);
  await loaded;

  const secondPageBrowsingContext = browser.browsingContext;
  if (isBfcacheInParentEnabled) {
    isnot(
      firstPageBrowsingContext,
      secondPageBrowsingContext,
      "With bfcache in parent, navigations spawn a new BrowsingContext"
    );
  } else {
    is(
      firstPageBrowsingContext,
      secondPageBrowsingContext,
      "Without bfcache in parent, navigations reuse the same BrowsingContext"
    );
  }
  {
    const { browsingContext, location, request, flags } =
      await onSecondLocationChanged;
    is(
      browsingContext,
      secondPageBrowsingContext,
      "Second location change fires on the new BrowsingContext"
    );
    ok(location instanceof Ci.nsIURI);
    is(location.spec, secondLocation);
    ok(request instanceof Ci.nsIChannel);
    is(request.URI.spec, secondLocation);
    is(flags, 0);
  }
  {
    const { browsingContext, request } = await onSecondPageDocumentStart;
    is(
      browsingContext,
      firstPageBrowsingContext,
      "STATE_START, when navigating to another process, fires on the BrowsingContext we navigate *from*"
    );
    ok(request instanceof Ci.nsIChannel);
    is(request.URI.spec, secondLocation);
  }

  const onBackLocationChanged = waitForNextLocationChange(webProgress, true);
  const onBackDocumentStart = waitForNextDocumentStart(webProgress);
  browser.goBack();

  {
    const { browsingContext, location, request, flags } =
      await onBackLocationChanged;
    is(
      browsingContext,
      firstPageBrowsingContext,
      "location change, when navigating back, fires on the BrowsingContext we navigate *to*"
    );
    ok(location instanceof Ci.nsIURI);
    is(location.spec, newLocation);
    ok(request instanceof Ci.nsIChannel);
    is(request.URI.spec, newLocation);
    is(flags, 0);
  }
  {
    const { browsingContext, request } = await onBackDocumentStart;
    is(
      browsingContext,
      secondPageBrowsingContext,
      "STATE_START, when navigating back, fires on the BrowsingContext we navigate *from*"
    );
    ok(request instanceof Ci.nsIChannel);
    is(request.URI.spec, newLocation);
  }

  BrowserTestUtils.removeTab(tab);
});

function waitForNextLocationChange(webProgress, onlyTopLevel = false) {
  return new Promise(resolve => {
    const wpl = {
      QueryInterface: ChromeUtils.generateQI([
        "nsIWebProgressListener",
        "nsISupportsWeakReference",
      ]),
      onLocationChange(progress, request, location, flags) {
        if (onlyTopLevel && progress.browsingContext.parent) {
          // Ignore non-toplevel.
          return;
        }
        webProgress.removeProgressListener(wpl, Ci.nsIWebProgress.NOTIFY_ALL);
        resolve({
          browsingContext: progress.browsingContext,
          location,
          request,
          flags,
        });
      },
    };
    // Add a strong reference to the progress listener.
    resolve.wpl = wpl;
    webProgress.addProgressListener(wpl, Ci.nsIWebProgress.NOTIFY_ALL);
  });
}

function waitForNextDocumentStart(webProgress) {
  return new Promise(resolve => {
    const wpl = {
      QueryInterface: ChromeUtils.generateQI([
        "nsIWebProgressListener",
        "nsISupportsWeakReference",
      ]),
      onStateChange(progress, request, flags, status) {
        if (
          flags & Ci.nsIWebProgressListener.STATE_IS_DOCUMENT &&
          flags & Ci.nsIWebProgressListener.STATE_START
        ) {
          webProgress.removeProgressListener(wpl, Ci.nsIWebProgress.NOTIFY_ALL);
          resolve({ browsingContext: progress.browsingContext, request });
        }
      },
    };
    // Add a strong reference to the progress listener.
    resolve.wpl = wpl;
    webProgress.addProgressListener(wpl, Ci.nsIWebProgress.NOTIFY_ALL);
  });
}
