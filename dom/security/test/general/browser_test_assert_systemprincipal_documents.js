//"use strict"

const kTestPath = getRootDirectory(gTestPath);
const kTestURI = kTestPath + "file_assert_systemprincipal_documents.html";

add_setup(async function () {
  // We expect the assertion in function
  // CheckSystemPrincipalLoads as defined in
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
      await SpecialPowers.spawn(browser, [], async function () {
        let outerPrincipal = content.document.nodePrincipal;
        ok(
          outerPrincipal.isSystemPrincipal,
          "Sanity: Using SystemPrincipal for test file on chrome://"
        );
        const iframeDoc =
          content.document.getElementById("testframe").contentDocument;
        is(
          iframeDoc.body.innerHTML,
          "",
          "iframe with systemprincipal should be empty document"
        );
      });
    }
  );
});
