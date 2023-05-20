// Test reverting the urlbar value with ESC after a tab switch.

add_task(async function () {
  registerCleanupFunction(PlacesUtils.history.clear);
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "http://example.com",
    },
    async function (browser) {
      let originalValue = gURLBar.value;
      let tab = gBrowser.selectedTab;
      info("Put a typed value.");
      gBrowser.userTypedValue = "foobar";
      info("Switch tabs.");
      gBrowser.selectedTab = gBrowser.tabs[0];
      gBrowser.selectedTab = tab;
      Assert.equal(
        gURLBar.value,
        "foobar",
        "location bar displays typed value"
      );

      gURLBar.focus();
      EventUtils.synthesizeKey("KEY_Escape");
      Assert.equal(
        gURLBar.value,
        originalValue,
        "ESC reverted the location bar value"
      );
    }
  );
});
