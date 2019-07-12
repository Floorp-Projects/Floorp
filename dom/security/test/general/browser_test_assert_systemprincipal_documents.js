//"use strict"

const kTestPath = getRootDirectory(gTestPath);
const kTestURI = kTestPath + "file_assert_systemprincipal_documents.html";

add_task(async function setup() {
  // We expect the assertion in function
  // AssertSystemPrincipalMustNotLoadRemoteDocuments as defined in
  // file dom/security/nsContentSecurityManager.cpp
  SimpleTest.expectAssertions(1);

  await SpecialPowers.pushPrefEnv({
    set: [
      ["security.disallow_non_local_systemprincipal_in_tests", true],
      ["security.allow_unsafe_parent_loads", true],
    ],
  });
});

add_task(async function open_test_iframe_in_tab() {
  // This looks at the iframe (load type SUBDOCUMENT)
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: kTestURI },
    async browser => {
      await ContentTask.spawn(browser, {}, async function() {
        let outerPrincipal = content.document.nodePrincipal;
        ok(
          outerPrincipal.isSystemPrincipal,
          "Sanity: Using SystemPrincipal for test file on chrome://"
        );

        const iframeWin = content.document.getElementById("testframe")
          .contentWindow;
        const iframeChannel = iframeWin.docShell.currentDocumentChannel;
        ok(
          iframeChannel.loadInfo.loadingPrincipal.isSystemPrincipal,
          "LoadingPrincipal for iframe is SystemPrincipal"
        );
      });
    }
  );
});
