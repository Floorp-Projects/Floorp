function test() {
  var tab = gBrowser.addTab("about:blank", { skipAnimation: true });
  gBrowser.removeTab(tab);
  is(tab.parentNode, null, "tab removed immediately");

  tab = gBrowser.addTab("about:blank", { skipAnimation: true });
  gBrowser.removeTab(tab, { animate: true });
  gBrowser.removeTab(tab);
  is(tab.parentNode, null, "tab removed immediately when calling removeTab again after the animation was kicked off");
}

