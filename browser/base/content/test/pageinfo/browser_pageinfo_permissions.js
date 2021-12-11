const { SitePermissions } = ChromeUtils.import(
  "resource:///modules/SitePermissions.jsm"
);
const { PermissionTestUtils } = ChromeUtils.import(
  "resource://testing-common/PermissionTestUtils.jsm"
);

const TEST_ORIGIN = "https://example.com";
const TEST_ORIGIN_CERT_ERROR = "https://expired.example.com";
const LOW_TLS_VERSION = "https://tls1.example.com/";

async function testPermissions(defaultPermission) {
  await BrowserTestUtils.withNewTab(TEST_ORIGIN, async function(browser) {
    let pageInfo = BrowserPageInfo(TEST_ORIGIN, "permTab");
    await BrowserTestUtils.waitForEvent(pageInfo, "load");

    let defaultCheckbox = await TestUtils.waitForCondition(() =>
      pageInfo.document.getElementById("geoDef")
    );
    let radioGroup = pageInfo.document.getElementById("geoRadioGroup");
    let defaultRadioButton = pageInfo.document.getElementById(
      "geo#" + defaultPermission
    );
    let blockRadioButton = pageInfo.document.getElementById("geo#2");

    ok(defaultCheckbox.checked, "The default checkbox should be checked.");

    PermissionTestUtils.add(
      gBrowser.currentURI,
      "geo",
      Services.perms.DENY_ACTION
    );

    ok(!defaultCheckbox.checked, "The default checkbox should not be checked.");

    defaultCheckbox.checked = true;
    defaultCheckbox.dispatchEvent(new Event("command"));

    ok(
      !PermissionTestUtils.getPermissionObject(gBrowser.currentURI, "geo"),
      "Checking the default checkbox should reset the permission."
    );

    defaultCheckbox.checked = false;
    defaultCheckbox.dispatchEvent(new Event("command"));

    ok(
      !PermissionTestUtils.getPermissionObject(gBrowser.currentURI, "geo"),
      "Unchecking the default checkbox should pick the default permission."
    );
    is(
      radioGroup.selectedItem,
      defaultRadioButton,
      "The unknown radio button should be selected."
    );

    radioGroup.selectedItem = blockRadioButton;
    blockRadioButton.dispatchEvent(new Event("command"));

    is(
      PermissionTestUtils.getPermissionObject(gBrowser.currentURI, "geo")
        .capability,
      Services.perms.DENY_ACTION,
      "Selecting a value in the radio group should set the corresponding permission"
    );

    radioGroup.selectedItem = defaultRadioButton;
    defaultRadioButton.dispatchEvent(new Event("command"));

    ok(
      !PermissionTestUtils.getPermissionObject(gBrowser.currentURI, "geo"),
      "Selecting the default value should reset the permission."
    );
    ok(defaultCheckbox.checked, "The default checkbox should be checked.");

    pageInfo.close();
    PermissionTestUtils.remove(gBrowser.currentURI, "geo");
  });
}

// Test displaying website permissions on certificate error pages.
add_task(async function test_CertificateError() {
  let browser;
  let pageLoaded;
  await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    () => {
      gBrowser.selectedTab = BrowserTestUtils.addTab(
        gBrowser,
        TEST_ORIGIN_CERT_ERROR
      );
      browser = gBrowser.selectedBrowser;
      pageLoaded = BrowserTestUtils.waitForErrorPage(browser);
    },
    false
  );

  await pageLoaded;

  let pageInfo = BrowserPageInfo(TEST_ORIGIN_CERT_ERROR, "permTab");
  await BrowserTestUtils.waitForEvent(pageInfo, "load");
  let permissionTab = pageInfo.document.getElementById("permTab");
  await TestUtils.waitForCondition(
    () => BrowserTestUtils.is_visible(permissionTab),
    "Permission tab should be visible."
  );

  let hostText = pageInfo.document.getElementById("hostText");
  let permList = pageInfo.document.getElementById("permList");
  let excludedPermissions = pageInfo.window.getExcludedPermissions();
  let permissions = SitePermissions.listPermissions().filter(
    p =>
      SitePermissions.getPermissionLabel(p) != null &&
      !excludedPermissions.includes(p)
  );

  await TestUtils.waitForCondition(
    () => hostText.value === browser.currentURI.displayPrePath,
    `Value of owner should be "${browser.currentURI.displayPrePath}" instead got "${hostText.value}".`
  );

  await TestUtils.waitForCondition(
    () => permList.childElementCount === permissions.length,
    `Value of verifier should be ${permissions.length}, instead got ${permList.childElementCount}.`
  );

  pageInfo.close();
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

// Test displaying website permissions on network error pages.
add_task(async function test_NetworkError() {
  // Setup for TLS error
  Services.prefs.setIntPref("security.tls.version.max", 3);
  Services.prefs.setIntPref("security.tls.version.min", 3);

  let browser;
  let pageLoaded;
  await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    () => {
      gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, LOW_TLS_VERSION);
      browser = gBrowser.selectedBrowser;
      pageLoaded = BrowserTestUtils.waitForErrorPage(browser);
    },
    false
  );

  await pageLoaded;

  let pageInfo = BrowserPageInfo(LOW_TLS_VERSION, "permTab");
  await BrowserTestUtils.waitForEvent(pageInfo, "load");
  let permissionTab = pageInfo.document.getElementById("permTab");
  await TestUtils.waitForCondition(
    () => BrowserTestUtils.is_visible(permissionTab),
    "Permission tab should be visible."
  );

  let hostText = pageInfo.document.getElementById("hostText");
  let permList = pageInfo.document.getElementById("permList");
  let excludedPermissions = pageInfo.window.getExcludedPermissions();
  let permissions = SitePermissions.listPermissions().filter(
    p =>
      SitePermissions.getPermissionLabel(p) != null &&
      !excludedPermissions.includes(p)
  );

  await TestUtils.waitForCondition(
    () => hostText.value === browser.currentURI.displayPrePath,
    `Value of host should be should be "${browser.currentURI.displayPrePath}" instead got "${hostText.value}".`
  );

  await TestUtils.waitForCondition(
    () => permList.childElementCount === permissions.length,
    `Value of permissions list should be ${permissions.length}, instead got ${permList.childElementCount}.`
  );

  pageInfo.close();
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

// Test some standard operations in the permission tab.
add_task(async function test_geo_permission() {
  await testPermissions(Services.perms.UNKNOWN_ACTION);
});

// Test some standard operations in the permission tab, falling back to a custom
// default permission instead of UNKNOWN.
add_task(async function test_default_geo_permission() {
  await SpecialPowers.pushPrefEnv({
    set: [["permissions.default.geo", SitePermissions.ALLOW]],
  });
  await testPermissions(Services.perms.ALLOW_ACTION);
});

// Test special behavior for cookie permissions.
add_task(async function test_cookie_permission() {
  await BrowserTestUtils.withNewTab(TEST_ORIGIN, async function(browser) {
    let pageInfo = BrowserPageInfo(TEST_ORIGIN, "permTab");
    await BrowserTestUtils.waitForEvent(pageInfo, "load");

    let defaultCheckbox = await TestUtils.waitForCondition(() =>
      pageInfo.document.getElementById("cookieDef")
    );
    let radioGroup = pageInfo.document.getElementById("cookieRadioGroup");
    let allowRadioButton = pageInfo.document.getElementById("cookie#1");
    let blockRadioButton = pageInfo.document.getElementById("cookie#2");

    ok(defaultCheckbox.checked, "The default checkbox should be checked.");

    defaultCheckbox.checked = false;
    defaultCheckbox.dispatchEvent(new Event("command"));

    is(
      PermissionTestUtils.testPermission(gBrowser.currentURI, "cookie"),
      SitePermissions.ALLOW,
      "Unchecking the default checkbox should pick the default permission."
    );
    is(
      radioGroup.selectedItem,
      allowRadioButton,
      "The unknown radio button should be selected."
    );

    radioGroup.selectedItem = blockRadioButton;
    blockRadioButton.dispatchEvent(new Event("command"));

    is(
      PermissionTestUtils.testPermission(gBrowser.currentURI, "cookie"),
      SitePermissions.BLOCK,
      "Selecting a value in the radio group should set the corresponding permission"
    );

    radioGroup.selectedItem = allowRadioButton;
    allowRadioButton.dispatchEvent(new Event("command"));

    is(
      PermissionTestUtils.testPermission(gBrowser.currentURI, "cookie"),
      SitePermissions.ALLOW,
      "Selecting a value in the radio group should set the corresponding permission"
    );
    ok(!defaultCheckbox.checked, "The default checkbox should not be checked.");

    defaultCheckbox.checked = true;
    defaultCheckbox.dispatchEvent(new Event("command"));

    is(
      PermissionTestUtils.testPermission(gBrowser.currentURI, "cookie"),
      SitePermissions.UNKNOWN,
      "Checking the default checkbox should reset the permission."
    );
    is(
      radioGroup.selectedItem,
      null,
      "For cookies, no item should be selected when the checkbox is checked."
    );

    pageInfo.close();
    PermissionTestUtils.remove(gBrowser.currentURI, "cookie");
  });
});
