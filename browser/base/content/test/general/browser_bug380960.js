function test() {
  var tab = BrowserTestUtils.addTab(gBrowser, "about:blank", {
    skipAnimation: true,
  });
  gBrowser.removeTab(tab);
  is(tab.parentNode, null, "tab removed immediately");

  tab = BrowserTestUtils.addTab(gBrowser, "about:blank", {
    skipAnimation: true,
  });
  gBrowser.removeTab(tab, { animate: true });
  gBrowser.removeTab(tab);
  is(
    tab.parentNode,
    null,
    "tab removed immediately when calling removeTab again after the animation was kicked off"
  );
}
