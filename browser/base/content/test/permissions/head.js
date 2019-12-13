ChromeUtils.import("resource:///modules/SitePermissions.jsm", this);
const { PermissionTestUtils } = ChromeUtils.import(
  "resource://testing-common/PermissionTestUtils.jsm"
);

SpecialPowers.addTaskImport(
  "E10SUtils",
  "resource://gre/modules/E10SUtils.jsm"
);
