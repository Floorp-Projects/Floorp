"use strict";

const kChildPage = getRootDirectory(gTestPath) + "file_about_child.html";

const kAboutPagesRegistered = BrowserTestUtils.registerAboutPage(
  registerCleanupFunction,
  "test-about-privileged-with-scripts",
  kChildPage,
  Ci.nsIAboutModule.ALLOW_SCRIPT |
    Ci.nsIAboutModule.URI_MUST_LOAD_IN_CHILD |
    Ci.nsIAboutModule.URI_CAN_LOAD_IN_PRIVILEGEDABOUT_PROCESS |
    Ci.nsIAboutModule.URI_SAFE_FOR_UNTRUSTED_CONTENT |
    Ci.nsIAboutModule.IS_SECURE_CHROME_UI
);

add_task(async function test_principal_click() {
  await kAboutPagesRegistered;
  await SpecialPowers.pushPrefEnv({
    set: [["dom.security.skip_about_page_has_csp_assert", true]],
  });
  await BrowserTestUtils.withNewTab(
    "about:test-about-privileged-with-scripts",
    async function (browser) {
      // Wait for page to fully load
      info("Waiting for tab to be loaded..");
      // let's look into the fully loaded about page
      await SpecialPowers.spawn(
        gBrowser.selectedBrowser,
        [],
        async function () {
          let channel = content.docShell.currentDocumentChannel;
          is(
            channel.originalURI.asciiSpec,
            "about:test-about-privileged-with-scripts",
            "sanity check - make sure we test the principal for the correct URI"
          );

          let triggeringPrincipal = channel.loadInfo.triggeringPrincipal;
          ok(
            triggeringPrincipal.isSystemPrincipal,
            "loading about: from privileged page must have a triggering of System"
          );

          let contentPolicyType = channel.loadInfo.externalContentPolicyType;
          is(
            contentPolicyType,
            Ci.nsIContentPolicy.TYPE_DOCUMENT,
            "sanity check - loading a top level document"
          );

          let loadingPrincipal = channel.loadInfo.loadingPrincipal;
          is(
            loadingPrincipal,
            null,
            "sanity check - load of TYPE_DOCUMENT must have a null loadingPrincipal"
          );
          ok(
            !content.document.nodePrincipal.isSystemPrincipal,
            "sanity check - loaded about page does not have the system principal"
          );
          isnot(
            content.testResult,
            "fail-script-was-loaded",
            "The script from https://example.com shouldn't work in an about: page."
          );
        }
      );
    }
  );
});
