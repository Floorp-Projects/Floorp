/*
 * Description of the Tests for
 *  - Bug 1204983 - Allow about: pages to load remote content
 *
 * We perform two tests:
 * (1) We load a new tab (about:newtab) using the default url and make sure that URL
 *     of the doucment matches about:newtab and the principal is the systemPrincipal.
 * (2) We load a new tab (about:newtab) and make sure that document.location as well
 *     as the nodePrincipal match the URL in the URL bar.
 */

/* globals Cc, Ci, ok, is, content, TestRunner, addNewTabPageTab, gWindow, Services, info */
/* exported runTests */

"use strict";

var aboutNewTabService = Cc["@mozilla.org/browser/aboutnewtab-service;1"]
                           .getService(Ci.nsIAboutNewTabService);

const ABOUT_NEWTAB_URI = "about:newtab";
const PREF_URI = "http://example.com/browser/browser/base/content/test/newtab/external_newtab.html";
const DEFAULT_URI = aboutNewTabService.newTabURL;

function* loadNewPageAndVerify(browser, uri) {
  let browserLoadedPromise = BrowserTestUtils.waitForEvent(browser, "load", true);
  browser.loadURI("about:newtab");
  yield browserLoadedPromise;

  yield ContentTask.spawn(gBrowser.selectedBrowser, { uri: uri }, function* (args) {
    let uri = args.uri;

    is(String(content.document.location), uri, "document.location should match " + uri);
    is(content.document.documentURI, uri, "document.documentURI should match " + uri);

    if (uri == "about:newtab") {
      is(content.document.nodePrincipal,
         Services.scriptSecurityManager.getSystemPrincipal(),
         "nodePrincipal should match systemPrincipal");
    }
    else {
      is(content.document.nodePrincipal.URI.spec, uri,
         "nodePrincipal should match " + uri);
    }
  }, true);
}

add_task(function* () {
  // test the default behavior
  yield* addNewTabPageTab();

  let browser = gBrowser.selectedBrowser;

  ok(!aboutNewTabService.overridden,
     "sanity check: default URL for about:newtab should not be overriden");

  yield* loadNewPageAndVerify(browser, ABOUT_NEWTAB_URI);

  // set the pref for about:newtab to point to an exteranl resource
  aboutNewTabService.newTabURL = PREF_URI;
  ok(aboutNewTabService.overridden,
     "sanity check: default URL for about:newtab should be overriden");
  is(aboutNewTabService.newTabURL, PREF_URI,
     "sanity check: default URL for about:newtab should return the new URL");

  yield* loadNewPageAndVerify(browser, PREF_URI);

  // reset to about:newtab and perform sanity check
  aboutNewTabService.resetNewTabURL();
  is(aboutNewTabService.newTabURL, DEFAULT_URI,
     "sanity check: resetting the URL to about:newtab should return about:newtab");

  // remove the tab and move on
  gBrowser.removeCurrentTab();
});
