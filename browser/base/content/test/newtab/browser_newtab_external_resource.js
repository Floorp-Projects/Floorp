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

var browser = null;
var aboutNewTabService = Cc["@mozilla.org/browser/aboutnewtab-service;1"]
                           .getService(Ci.nsIAboutNewTabService);

const ABOUT_NEWTAB_URI = "about:newtab";
const PREF_URI = "http://example.com/browser/browser/base/content/test/newtab/external_newtab.html";
const DEFAULT_URI = aboutNewTabService.newTabURL;

function testPref() {
  // set the pref for about:newtab to point to an exteranl resource
  aboutNewTabService.newTabURL = PREF_URI;
  ok(aboutNewTabService.overridden,
     "sanity check: default URL for about:newtab should be overriden");
  is(aboutNewTabService.newTabURL, PREF_URI,
     "sanity check: default URL for about:newtab should return the new URL");

  browser.contentWindow.location = ABOUT_NEWTAB_URI;

  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    is(content.document.location, PREF_URI, "document.location should match the external resource");
    is(content.document.documentURI, PREF_URI, "document.documentURI should match the external resource");
    is(content.document.nodePrincipal.URI.spec, PREF_URI, "nodePrincipal should match the external resource");

    // reset to about:newtab and perform sanity check
    aboutNewTabService.resetNewTabURL();
    is(aboutNewTabService.newTabURL, DEFAULT_URI,
       "sanity check: resetting the URL to about:newtab should return about:newtab");

    // remove the tab and move on
    gBrowser.removeCurrentTab();
    TestRunner.next();
  }, true);
}

function runTests() {
  // test the default behavior
  yield addNewTabPageTab();
  browser = gWindow.gBrowser.selectedBrowser;

  ok(!aboutNewTabService.overridden,
     "sanity check: default URL for about:newtab should not be overriden");
  browser.contentWindow.location = ABOUT_NEWTAB_URI;

  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    is(content.document.location, ABOUT_NEWTAB_URI, "document.location should match about:newtab");
    is(content.document.documentURI, ABOUT_NEWTAB_URI, "document.documentURI should match about:newtab");
    is(content.document.nodePrincipal,
       Services.scriptSecurityManager.getSystemPrincipal(),
       "nodePrincipal should match systemPrincipal");

    // also test the pref
    testPref();
  }, true);

  info("Waiting for about:newtab to load ...");
  yield true;
}
