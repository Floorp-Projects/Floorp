ChromeUtils.import("resource:///modules/SitePermissions.jsm");

const TEST_ORIGIN = "https://example.com";

async function testPermissions(defaultPermission) {
  await BrowserTestUtils.withNewTab(TEST_ORIGIN, async function(browser) {
    let pageInfo = BrowserPageInfo(TEST_ORIGIN, "permTab");
    await BrowserTestUtils.waitForEvent(pageInfo, "load");

    let defaultCheckbox = await TestUtils.waitForCondition(() => pageInfo.document.getElementById("geoDef"));
    let radioGroup = pageInfo.document.getElementById("geoRadioGroup");
    let defaultRadioButton = pageInfo.document.getElementById("geo#" + defaultPermission);
    let blockRadioButton = pageInfo.document.getElementById("geo#2");

    ok(defaultCheckbox.checked, "The default checkbox should be checked.");

    SitePermissions.set(gBrowser.currentURI, "geo", SitePermissions.BLOCK);

    ok(!defaultCheckbox.checked, "The default checkbox should not be checked.");

    defaultCheckbox.checked = true;
    defaultCheckbox.dispatchEvent(new Event("command"));

    is(SitePermissions.get(gBrowser.currentURI, "geo").state, defaultPermission,
      "Checking the default checkbox should reset the permission.");

    defaultCheckbox.checked = false;
    defaultCheckbox.dispatchEvent(new Event("command"));

    is(SitePermissions.get(gBrowser.currentURI, "geo").state, defaultPermission,
      "Unchecking the default checkbox should pick the default permission.");
    is(radioGroup.selectedItem, defaultRadioButton,
      "The unknown radio button should be selected.");

    radioGroup.selectedItem = blockRadioButton;
    blockRadioButton.dispatchEvent(new Event("command"));

    is(SitePermissions.get(gBrowser.currentURI, "geo").state, SitePermissions.BLOCK,
      "Selecting a value in the radio group should set the corresponding permission");

    radioGroup.selectedItem = defaultRadioButton;
    defaultRadioButton.dispatchEvent(new Event("command"));

    is(SitePermissions.get(gBrowser.currentURI, "geo").state, defaultPermission,
      "Selecting the default value should reset the permission.");
    ok(defaultCheckbox.checked, "The default checkbox should be checked.");

    pageInfo.close();
    SitePermissions.remove(gBrowser.currentURI, "geo");
  });
}

// Test some standard operations in the permission tab.
add_task(async function test_geo_permission() {
  await testPermissions(SitePermissions.UNKNOWN);
});

// Test some standard operations in the permission tab, falling back to a custom
// default permission instead of UNKNOWN.
add_task(async function test_default_geo_permission() {
  await SpecialPowers.pushPrefEnv({set: [["permissions.default.geo", SitePermissions.ALLOW]]});
  await testPermissions(SitePermissions.ALLOW);
});

// Test special behavior for cookie permissions.
add_task(async function test_cookie_permission() {
  await BrowserTestUtils.withNewTab(TEST_ORIGIN, async function(browser) {
    let pageInfo = BrowserPageInfo(TEST_ORIGIN, "permTab");
    await BrowserTestUtils.waitForEvent(pageInfo, "load");

    let defaultCheckbox = await TestUtils.waitForCondition(() => pageInfo.document.getElementById("cookieDef"));
    let radioGroup = pageInfo.document.getElementById("cookieRadioGroup");
    let allowRadioButton = pageInfo.document.getElementById("cookie#1");
    let blockRadioButton = pageInfo.document.getElementById("cookie#2");

    ok(defaultCheckbox.checked, "The default checkbox should be checked.");

    defaultCheckbox.checked = false;
    defaultCheckbox.dispatchEvent(new Event("command"));

    is(Services.perms.testPermission(gBrowser.currentURI, "cookie"), SitePermissions.ALLOW,
      "Unchecking the default checkbox should pick the default permission.");
    is(radioGroup.selectedItem, allowRadioButton,
      "The unknown radio button should be selected.");

    radioGroup.selectedItem = blockRadioButton;
    blockRadioButton.dispatchEvent(new Event("command"));

    is(Services.perms.testPermission(gBrowser.currentURI, "cookie"), SitePermissions.BLOCK,
      "Selecting a value in the radio group should set the corresponding permission");

    radioGroup.selectedItem = allowRadioButton;
    allowRadioButton.dispatchEvent(new Event("command"));

    is(Services.perms.testPermission(gBrowser.currentURI, "cookie"), SitePermissions.ALLOW,
      "Selecting a value in the radio group should set the corresponding permission");
    ok(!defaultCheckbox.checked, "The default checkbox should not be checked.");

    defaultCheckbox.checked = true;
    defaultCheckbox.dispatchEvent(new Event("command"));

    is(Services.perms.testPermission(gBrowser.currentURI, "cookie"), SitePermissions.UNKNOWN,
      "Checking the default checkbox should reset the permission.");
    is(radioGroup.selectedItem, null, "For cookies, no item should be selected when the checkbox is checked.");

    pageInfo.close();
    SitePermissions.remove(gBrowser.currentURI, "cookie");
  });
});
