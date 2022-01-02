/**
 * Bug 1282655 - Test if site permissions are universal across origin attributes.
 *
 * This test is testing the cookie "permission" for a specific URI.
 */

const { PermissionTestUtils } = ChromeUtils.import(
  "resource://testing-common/PermissionTestUtils.jsm"
);

const TEST_PAGE = "http://example.net";
const uri = Services.io.newURI(TEST_PAGE);

async function disableCookies() {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.security.https_first", false]],
  });
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
  // Bug 1617611: Fix all the tests broken by "cookies SameSite=lax by default"
  await SpecialPowers.pushPrefEnv({
    set: [["network.cookie.sameSite.laxByDefault", false]],
  });
  await SpecialPowers.spawn(aBrowser, [], async function() {
    content.document.cookie = "key=value";
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
  await SpecialPowers.pushPrefEnv({
    set: [["dom.security.https_first", false]],
  });
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
  // Bug 1617611: Fix all the tests broken by "cookies SameSite=lax by default"
  await SpecialPowers.pushPrefEnv({
    set: [["network.cookie.sameSite.laxByDefault", false]],
  });
  await SpecialPowers.spawn(aBrowser, [], function() {
    content.document.cookie = "key=value";
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
