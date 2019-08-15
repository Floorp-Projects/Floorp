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

function disableCookies() {
  Services.cookies.removeAll();
  PermissionTestUtils.add(uri, "cookie", Services.perms.DENY_ACTION);
}

function ensureCookieNotSet(aBrowser) {
  ContentTask.spawn(aBrowser, null, async function() {
    content.document.cookie = "key=value";
    is(
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

function enableCookies() {
  Services.cookies.removeAll();
  PermissionTestUtils.add(uri, "cookie", Services.perms.ALLOW_ACTION);
}

function ensureCookieSet(aBrowser) {
  ContentTask.spawn(aBrowser, null, function() {
    content.document.cookie = "key=value";
    is(
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
  Services.cookies.removeAll();
});
