/**
 * Bug 1282655 - Test if site permissions are universal across origin attributes.
 *
 * This test is testing the cookie "permission" for a specific URI.
 */

const { PermissionTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PermissionTestUtils.sys.mjs"
);

const TEST_PAGE = "https://example.net";
const uri = Services.io.newURI(TEST_PAGE);

async function disableCookies() {
  Services.cookies.removeAll();
  PermissionTestUtils.add(uri, "cookie", Services.perms.DENY_ACTION);

  // A workaround for making this test working. In Bug 1330467, we separate the
  // permissions between different firstPartyDomains, but not for the
  // userContextID and the privateBrowsingId. So we need to manually add the
  // permission for FPDs in order to make this test working. This test should be
  // eventually removed once the permissions are isolated by OAs.
  let principal = Services.scriptSecurityManager.createContentPrincipal(uri, {
    firstPartyDomain: "example.com",
  });
  PermissionTestUtils.add(principal, "cookie", Services.perms.DENY_ACTION);

  principal = Services.scriptSecurityManager.createContentPrincipal(uri, {
    firstPartyDomain: "example.org",
  });
  PermissionTestUtils.add(principal, "cookie", Services.perms.DENY_ACTION);
}

async function ensureCookieNotSet(aBrowser) {
  await SpecialPowers.spawn(aBrowser, [], async function () {
    content.document.cookie = "key=value; SameSite=None; Secure;";
    Assert.equal(
      content.document.cookie,
      "",
      "Setting/reading cookies should be disabled" +
        " for this domain for all origin attribute combinations."
    );
  });
}

IsolationTestTools.runTests(
  TEST_PAGE,
  ensureCookieNotSet,
  () => true,
  disableCookies
);

async function enableCookies() {
  Services.cookies.removeAll();
  PermissionTestUtils.add(uri, "cookie", Services.perms.ALLOW_ACTION);

  // A workaround for making this test working.
  let principal = Services.scriptSecurityManager.createContentPrincipal(uri, {
    firstPartyDomain: "example.com",
  });
  PermissionTestUtils.add(principal, "cookie", Services.perms.ALLOW_ACTION);

  principal = Services.scriptSecurityManager.createContentPrincipal(uri, {
    firstPartyDomain: "example.org",
  });
  PermissionTestUtils.add(principal, "cookie", Services.perms.ALLOW_ACTION);
}

async function ensureCookieSet(aBrowser) {
  await SpecialPowers.spawn(aBrowser, [], function () {
    content.document.cookie = "key=value; SameSite=None; Secure;";
    Assert.equal(
      content.document.cookie,
      "key=value",
      "Setting/reading cookies should be" +
        " enabled for this domain for all origin attribute combinations."
    );
  });
}

IsolationTestTools.runTests(
  TEST_PAGE,
  ensureCookieSet,
  () => true,
  enableCookies
);

registerCleanupFunction(() => {
  SpecialPowers.clearUserPref("network.cookie.sameSite.laxByDefault");
  Services.cookies.removeAll();
});
