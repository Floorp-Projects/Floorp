/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function() {
  const tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank"
  );
  const { browsingContext } = tab.linkedBrowser;
  const { webProgress } = browsingContext;
  ok(
    webProgress,
    "Got a WebProgress interface on BrowsingContext in the parent process"
  );
  is(
    webProgress.browsingContext,
    browsingContext,
    "WebProgress.browsingContext refers to the right BrowsingContext"
  );

  const onLocationChanged = waitForNextLocationChange(webProgress);
  const newLocation = "data:text/html;charset=utf-8,foo";
  BrowserTestUtils.loadURI(tab.linkedBrowser, newLocation);
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  info("Wait for onLocationChange to be fired");
  {
    const { progress, location, request, flags } = await onLocationChanged;
    is(progress, webProgress, "Location change fired on the same WebProgress");
    ok(location instanceof Ci.nsIURI);
    is(location.spec, newLocation);
    ok(request instanceof Ci.nsIChannel);
    is(request.URI.spec, newLocation);
    is(flags, 0);
  }

  const onIframeLocationChanged = waitForNextLocationChange(webProgress);
  const iframeLocation = "data:text/html;charset=utf-8,bar";
  const iframeBC = await SpecialPowers.spawn(
    tab.linkedBrowser,
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
    const {
      progress,
      location,
      request,
      flags,
    } = await onIframeLocationChanged;
    is(
      progress,
      iframeBC.webProgress,
      "Iframe location change fires on the iframe WebProgress"
    );
    ok(location instanceof Ci.nsIURI);
    is(location.spec, iframeLocation);
    ok(request instanceof Ci.nsIChannel);
    is(request.URI.spec, iframeLocation);
    is(flags, 0);
  }
  BrowserTestUtils.removeTab(tab);
});

function waitForNextLocationChange(webProgress) {
  return new Promise(resolve => {
    const wpl = {
      QueryInterface: ChromeUtils.generateQI([
        "nsIWebProgressListener",
        "nsISupportsWeakReference",
      ]),
      onLocationChange(progress, request, location, flags) {
        webProgress.removeProgressListener(wpl, Ci.nsIWebProgress.NOTIFY_ALL);
        resolve({ progress, location, request, flags });
      },
    };
    webProgress.addProgressListener(wpl, Ci.nsIWebProgress.NOTIFY_ALL);
  });
}
