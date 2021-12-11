/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const HTTPS_ONLY_PERMISSION = "https-only-load-insecure";
const WEBSITE = scheme => `${scheme}://example.com`;

add_task(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.security.https_only_mode", true]],
  });

  // Site is already HTTPS, so the UI should not be visible.
  await runTest({
    name: "No HTTPS-Only UI",
    initialScheme: "https",
    initialPermission: 0,
    permissionScheme: "https",
    isUiVisible: false,
  });

  // Site gets upgraded to HTTPS, so the UI should be visible.
  // Disabling HTTPS-Only Mode through the menulist should reload the page and
  // set the permission accordingly.
  await runTest({
    name: "Disable HTTPS-Only",
    initialScheme: "http",
    initialPermission: 0,
    permissionScheme: "https",
    isUiVisible: true,
    selectPermission: 1,
    expectReload: true,
    finalScheme: "https",
  });

  // HTTPS-Only Mode is disabled for this site, so the UI should be visible.
  // Disabling HTTPS-Only Mode through the menulist should not reload the page
  // but set the permission accordingly.
  await runTest({
    name: "Switch between off states",
    initialScheme: "http",
    initialPermission: 1,
    permissionScheme: "http",
    isUiVisible: true,
    selectPermission: 2,
    expectReload: false,
    finalScheme: "http",
  });

  // HTTPS-Only Mode is disabled for this site, so the UI should be visible.
  // Enabling HTTPS-Only Mode through the menulist should reload and upgrade the
  // page and set the permission accordingly.
  await runTest({
    name: "Enable HTTPS-Only again",
    initialScheme: "http",
    initialPermission: 2,
    permissionScheme: "http",
    isUiVisible: true,
    selectPermission: 0,
    expectReload: true,
    finalScheme: "https",
  });
});

async function runTest(options) {
  // Set the initial permission
  setPermission(WEBSITE(options.permissionScheme), options.initialPermission);

  await BrowserTestUtils.withNewTab(
    WEBSITE(options.initialScheme),
    async function(browser) {
      const name = options.name + " | ";

      // Check if the site has the expected scheme
      is(
        browser.currentURI.scheme,
        options.permissionScheme,
        name + "Expected scheme should match actual scheme"
      );

      // Open the identity popup.
      let { gIdentityHandler } = gBrowser.ownerGlobal;
      let promisePanelOpen = BrowserTestUtils.waitForEvent(
        gBrowser.ownerGlobal,
        "popupshown",
        true,
        event => event.target == gIdentityHandler._identityPopup
      );
      gIdentityHandler._identityIconBox.click();
      await promisePanelOpen;

      // Check if the HTTPS-Only UI is visible
      const httpsOnlyUI = document.getElementById(
        "identity-popup-security-httpsonlymode"
      );
      is(
        gBrowser.ownerGlobal.getComputedStyle(httpsOnlyUI).display != "none",
        options.isUiVisible,
        options.isUiVisible
          ? name + "HTTPS-Only UI should be visible."
          : name + "HTTPS-Only UI shouldn't be visible."
      );

      // If it's not visible we can't do much else :)
      if (!options.isUiVisible) {
        return;
      }

      // Check if the value of the menulist matches the initial permission
      const httpsOnlyMenulist = document.getElementById(
        "identity-popup-security-httpsonlymode-menulist"
      );
      is(
        parseInt(httpsOnlyMenulist.value, 10),
        options.initialPermission,
        name + "Menulist value should match expected permission value."
      );

      // Select another HTTPS-Only state and potentially wait for the page to reload
      if (options.expectReload) {
        const loaded = BrowserTestUtils.browserLoaded(browser);
        httpsOnlyMenulist.getItemAtIndex(options.selectPermission).doCommand();
        await loaded;
      } else {
        httpsOnlyMenulist.getItemAtIndex(options.selectPermission).doCommand();
      }

      // Check if the site has the expected scheme
      is(
        browser.currentURI.scheme,
        options.finalScheme,
        name + "Unexpected scheme after page reloaded."
      );

      // Check if the permission was sucessfully changed
      is(
        getPermission(WEBSITE(options.permissionScheme)),
        options.selectPermission,
        name + "Set permission should match the one selected from the menulist."
      );
    }
  );

  // Reset permission
  Services.perms.removeAll();
}

function setPermission(url, newValue) {
  let uri = Services.io.newURI(url);
  let principal = Services.scriptSecurityManager.createContentPrincipal(
    uri,
    {}
  );
  if (newValue === 0) {
    Services.perms.removeFromPrincipal(principal, HTTPS_ONLY_PERMISSION);
  } else if (newValue === 1) {
    Services.perms.addFromPrincipal(
      principal,
      HTTPS_ONLY_PERMISSION,
      Ci.nsIHttpsOnlyModePermission.LOAD_INSECURE_ALLOW,
      Ci.nsIPermissionManager.EXPIRE_NEVER
    );
  } else {
    Services.perms.addFromPrincipal(
      principal,
      HTTPS_ONLY_PERMISSION,
      Ci.nsIHttpsOnlyModePermission.LOAD_INSECURE_ALLOW_SESSION,
      Ci.nsIPermissionManager.EXPIRE_SESSION
    );
  }
}

function getPermission(url) {
  let uri = Services.io.newURI(url);
  let principal = Services.scriptSecurityManager.createContentPrincipal(
    uri,
    {}
  );
  const state = Services.perms.testPermissionFromPrincipal(
    principal,
    HTTPS_ONLY_PERMISSION
  );
  switch (state) {
    case Ci.nsIHttpsOnlyModePermission.LOAD_INSECURE_ALLOW_SESSION:
      return 2; // Off temporarily
    case Ci.nsIHttpsOnlyModePermission.LOAD_INSECURE_ALLOW:
      return 1; // Off
    default:
      return 0; // On
  }
}
