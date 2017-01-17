"use strict";
Cu.import("resource://gre/modules/Services.jsm");

registerCleanupFunction(function() {
  Services.ppmm.broadcastAsyncMessage("AboutPrincipalTest:Unregister");
  yield BrowserTestUtils.waitForMessage(Services.ppmm, "AboutPrincipalTest:Unregistered");

  Services.ppmm.removeDelayedProcessScript(
    "chrome://mochitests/content/browser/browser/base/content/test/general/file_register_about_page.js"
  );
});

add_task(function* test_principal() {
  Services.ppmm.loadProcessScript(
    "chrome://mochitests/content/browser/browser/base/content/test/general/file_register_about_page.js",
    true
  );

  yield BrowserTestUtils.withNewTab("about:test-about-principal-parent", function*(browser) {
    let loadPromise = BrowserTestUtils.browserLoaded(browser, false, "about:test-about-principal-child");
    let myLink = browser.contentDocument.getElementById("aboutchildprincipal");
    myLink.click();
    yield loadPromise;

    yield ContentTask.spawn(gBrowser.selectedBrowser, {}, function*() {
      let channel = content.document.docShell.currentDocumentChannel;
      is(channel.originalURI.asciiSpec,
         "about:test-about-principal-child",
         "sanity check - make sure we test the principal for the correct URI");

      let triggeringPrincipal = channel.loadInfo.triggeringPrincipal;
      ok(Services.scriptSecurityManager.isSystemPrincipal(triggeringPrincipal),
         "loading about: from privileged page must have a triggering of System");

      let contentPolicyType = channel.loadInfo.externalContentPolicyType;
      is(contentPolicyType, Ci.nsIContentPolicy.TYPE_DOCUMENT,
        "sanity check - loading a top level document");

      let loadingPrincipal = channel.loadInfo.loadingPrincipal;
      is(loadingPrincipal, null,
         "sanity check - load of TYPE_DOCUMENT must have a null loadingPrincipal");
    });
  });
});
