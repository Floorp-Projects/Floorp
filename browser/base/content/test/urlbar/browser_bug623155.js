/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const REDIRECT_FROM = "https://example.com/browser/browser/base/content/test/urlbar/" +
                      "redirect_bug623155.sjs";

const REDIRECT_TO = "https://www.bank1.com/"; // Bad-cert host.

function isRedirectedURISpec(aURISpec) {
  return isRedirectedURI(Services.io.newURI(aURISpec, null, null));
}

function isRedirectedURI(aURI) {
  // Compare only their before-hash portion.
  return Services.io.newURI(REDIRECT_TO, null, null)
                 .equalsExceptRef(aURI);
}

/*
   Test.

1. Load
https://example.com/browser/browser/base/content/test/urlbar/redirect_bug623155.sjs#BG
   in a background tab.

2. The redirected URI is <https://www.bank1.com/#BG>, which displayes a cert
   error page.

3. Switch the tab to foreground.

4. Check the URLbar's value, expecting <https://www.bank1.com/#BG>

5. Load
https://example.com/browser/browser/base/content/test/urlbar/redirect_bug623155.sjs#FG
   in the foreground tab.

6. The redirected URI is <https://www.bank1.com/#FG>. And this is also
   a cert-error page.

7. Check the URLbar's value, expecting <https://www.bank1.com/#FG>

8. End.

 */

var gNewTab;

function test() {
  waitForExplicitFinish();

  // Load a URI in the background.
  gNewTab = gBrowser.addTab(REDIRECT_FROM + "#BG");
  gBrowser.getBrowserForTab(gNewTab)
          .webProgress
          .addProgressListener(gWebProgressListener,
                               Components.interfaces.nsIWebProgress
                                                    .NOTIFY_LOCATION);
}

var gWebProgressListener = {
  QueryInterface: function(aIID) {
    if (aIID.equals(Components.interfaces.nsIWebProgressListener) ||
        aIID.equals(Components.interfaces.nsISupportsWeakReference) ||
        aIID.equals(Components.interfaces.nsISupports))
      return this;
    throw Components.results.NS_NOINTERFACE;
  },

  // ---------------------------------------------------------------------------
  // NOTIFY_LOCATION mode should work fine without these methods.
  //
  // onStateChange: function() {},
  // onStatusChange: function() {},
  // onProgressChange: function() {},
  // onSecurityChange: function() {},
  // ----------------------------------------------------------------------------

  onLocationChange: function(aWebProgress, aRequest, aLocation, aFlags) {
    if (!aRequest) {
      // This is bug 673752, or maybe initial "about:blank".
      return;
    }

    ok(gNewTab, "There is a new tab.");
    ok(isRedirectedURI(aLocation),
       "onLocationChange catches only redirected URI.");

    if (aLocation.ref == "BG") {
      // This is background tab's request.
      isnot(gNewTab, gBrowser.selectedTab, "This is a background tab.");
    } else if (aLocation.ref == "FG") {
      // This is foreground tab's request.
      is(gNewTab, gBrowser.selectedTab, "This is a foreground tab.");
    }
    else {
      // We shonuld not reach here.
      ok(false, "This URI hash is not expected:" + aLocation.ref);
    }

    let isSelectedTab = gNewTab.selected;
    setTimeout(delayed, 0, isSelectedTab);
  }
};

function delayed(aIsSelectedTab) {
  // Switch tab and confirm URL bar.
  if (!aIsSelectedTab) {
    gBrowser.selectedTab = gNewTab;
  }

  ok(isRedirectedURISpec(content.location.href),
     "The content area is redirected. aIsSelectedTab:" + aIsSelectedTab);
  is(gURLBar.value, content.location.href,
     "The URL bar shows the content URI. aIsSelectedTab:" + aIsSelectedTab);

  if (!aIsSelectedTab) {
    // If this was a background request, go on a foreground request.
    gBrowser.selectedBrowser.loadURI(REDIRECT_FROM + "#FG");
  }
  else {
    // Othrewise, nothing to do remains.
    finish();
  }
}

/* Cleanup */
registerCleanupFunction(function() {
  if (gNewTab) {
    gBrowser.getBrowserForTab(gNewTab)
            .webProgress
            .removeProgressListener(gWebProgressListener);

    gBrowser.removeTab(gNewTab);
  }
  gNewTab = null;
});
