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
