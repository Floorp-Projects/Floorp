/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const RELATIVE_DIR = "browser/app/profile/extensions/uriloader@pdf.js/test/";
const TESTROOT = "http://example.com/browser/" + RELATIVE_DIR;

function test() {
  waitForExplicitFinish();

  AddonManager.getAddonByID("uriloader@pdf.js", function(aAddon) {
    is(aAddon.userDisabled, true, 'Pdf.js addon must be disabled by default');
    aAddon.userDisabled = false;

    registerCleanupFunction(function() {
      aAddon.userDisabled = true;
    });

    continueTest();
  });
}

function continueTest() {
  var tab = gBrowser.addTab(TESTROOT + "file_pdfjs_test.pdf");
  var newTabBrowser = gBrowser.getBrowserForTab(tab);
  newTabBrowser.addEventListener("load", function onLoad() {
    newTabBrowser.removeEventListener("load", onLoad, true);

    var hasViewer = newTabBrowser.contentDocument.querySelector('div#viewer');
    var hasPDFJS = 'PDFJS' in newTabBrowser.contentWindow.wrappedJSObject;

    ok(hasViewer, "document content has viewer UI");
    ok(hasPDFJS, "window content has PDFJS object");

    finish();
  }, true, true);

  registerCleanupFunction(function() {
    gBrowser.removeTab(tab);
  });
}
