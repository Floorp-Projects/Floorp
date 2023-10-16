/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const EMPTY_PAGE =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "https://example.com"
  ) + "empty.html";

const SUBDOMAIN_EMPTY_PAGE =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "https://www.example.com"
  ) + "empty.html";

add_task(async function testSiteScopedPermissionSubdomainAffectsBaseDomain() {
  let subdomainOrigin = "https://www.example.com";
  let subdomainPrincipal =
    Services.scriptSecurityManager.createContentPrincipalFromOrigin(
      subdomainOrigin
    );
  let id = "3rdPartyStorage^https://example.org";

  await BrowserTestUtils.withNewTab(EMPTY_PAGE, async function (browser) {
    Services.perms.addFromPrincipal(
      subdomainPrincipal,
      id,
      SitePermissions.ALLOW
    );

    await openPermissionPopup();

    let permissionsList = document.getElementById(
      "permission-popup-permission-list"
    );
    let listEntryCount = permissionsList.querySelectorAll(
      ".permission-popup-permission-item"
    ).length;
    is(
      listEntryCount,
      1,
      "Permission exists on base domain when set on subdomain"
    );

    closePermissionPopup();

    Services.perms.removeFromPrincipal(subdomainPrincipal, id);

    // We intentionally turn off a11y_checks, because the following function
    // is expected to click a toolbar button that may be already hidden
    // with "display:none;". The permissions panel anchor is hidden because
    // the last permission was removed, however we force opening the panel
    // anyways in order to test that the list has been properly emptied:
    AccessibilityUtils.setEnv({
      mustHaveAccessibleRule: false,
    });
    await openPermissionPopup();
    AccessibilityUtils.resetEnv();

    listEntryCount = permissionsList.querySelectorAll(
      ".permission-popup-permission-item-3rdPartyStorage"
    ).length;
    is(
      listEntryCount,
      0,
      "Permission removed on base domain when removed on subdomain"
    );

    await closePermissionPopup();
  });
});

add_task(async function testSiteScopedPermissionBaseDomainAffectsSubdomain() {
  let origin = "https://example.com";
  let principal =
    Services.scriptSecurityManager.createContentPrincipalFromOrigin(origin);
  let id = "3rdPartyStorage^https://example.org";

  await BrowserTestUtils.withNewTab(
    SUBDOMAIN_EMPTY_PAGE,
    async function (browser) {
      Services.perms.addFromPrincipal(principal, id, SitePermissions.ALLOW);
      await openPermissionPopup();

      let permissionsList = document.getElementById(
        "permission-popup-permission-list"
      );
      let listEntryCount = permissionsList.querySelectorAll(
        ".permission-popup-permission-item"
      ).length;
      is(
        listEntryCount,
        1,
        "Permission exists on base domain when set on subdomain"
      );

      closePermissionPopup();

      Services.perms.removeFromPrincipal(principal, id);

      await openPermissionPopup();

      listEntryCount = permissionsList.querySelectorAll(
        ".permission-popup-permission-item-3rdPartyStorage"
      ).length;
      is(
        listEntryCount,
        0,
        "Permission removed on base domain when removed on subdomain"
      );

      await closePermissionPopup();
    }
  );
});
