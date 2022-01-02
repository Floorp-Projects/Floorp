const { SitePermissions } = ChromeUtils.import(
  "resource:///modules/SitePermissions.jsm"
);
const { PermissionTestUtils } = ChromeUtils.import(
  "resource://testing-common/PermissionTestUtils.jsm"
);

SpecialPowers.addTaskImport(
  "E10SUtils",
  "resource://gre/modules/E10SUtils.jsm"
);

function openPermissionPopup() {
  let promise = BrowserTestUtils.waitForEvent(
    gBrowser.ownerGlobal,
    "popupshown",
    true,
    event => event.target == gPermissionPanel._permissionPopup
  );
  gPermissionPanel._identityPermissionBox.click();
  return promise;
}

function closePermissionPopup() {
  let promise = BrowserTestUtils.waitForEvent(
    gPermissionPanel._permissionPopup,
    "popuphidden"
  );
  gPermissionPanel._permissionPopup.hidePopup();
  return promise;
}
