function test() {
  waitForExplicitFinish();

  let tab = gBrowser.addTab("http://example.com");
  gBrowser.selectedTab = tab;

  onLoad(function () {
    let originalValue = gURLBar.value;

    gBrowser.userTypedValue = "foobar";
    gBrowser.selectedTab = gBrowser.tabs[0];
    gBrowser.selectedTab = tab;
    is(gURLBar.value, "foobar", "location bar displays typed value");

    gURLBar.focus();
    EventUtils.synthesizeKey("VK_ESCAPE", {});
    is(gURLBar.value, originalValue, "ESC reverted the location bar value");

    gBrowser.removeTab(tab);
    finish();
  });
}

function onLoad(callback) {
  gBrowser.selectedBrowser.addEventListener("pageshow", function loadListener() {
    gBrowser.selectedBrowser.removeEventListener("pageshow", loadListener, false);
    executeSoon(callback);
  });
}
