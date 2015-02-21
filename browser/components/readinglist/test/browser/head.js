XPCOMUtils.defineLazyModuleGetter(this, "ReadingList",
                                  "resource:///modules/readinglist/ReadingList.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ReadingListTestUtils",
                                  "resource://testing-common/ReadingListTestUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "BrowserUITestUtils",
                                  "resource://testing-common/BrowserUITestUtils.jsm");


XPCOMUtils.defineLazyGetter(this, "RLUtils", () => {
  return ReadingListTestUtils;
});

XPCOMUtils.defineLazyGetter(this, "RLSidebarUtils", () => {
  return new RLUtils.SidebarUtils(window, Assert);
});
