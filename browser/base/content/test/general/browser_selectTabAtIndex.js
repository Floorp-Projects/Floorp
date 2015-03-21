function test() {
  for (let i = 0; i < 9; i++)
    gBrowser.addTab();

  var isLinux = navigator.platform.indexOf("Linux") == 0;
  for (let i = 9; i >= 1; i--) {
    EventUtils.synthesizeKey(i.toString(), { altKey: isLinux, accelKey: !isLinux });

    is(gBrowser.tabContainer.selectedIndex, (i == 9 ? gBrowser.tabs.length : i) - 1,
       (isLinux ? "Alt" : "Accel") + "+" + i + " selects expected tab");
  }

  gBrowser.selectTabAtIndex(-3);
  is(gBrowser.tabContainer.selectedIndex, gBrowser.tabs.length - 3,
     "gBrowser.selectTabAtIndex(-3) selects expected tab");

  for (let i = 0; i < 9; i++)
    gBrowser.removeCurrentTab();
}
