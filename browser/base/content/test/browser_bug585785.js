function test() {
  waitForExplicitFinish();

  var tab = gBrowser.addTab();
  isnot(tab.getAttribute("fadein"), "true", "newly opened tab is yet to fade in");

  // Remove the tab right before the opening animation's first frame
  window.mozRequestAnimationFrame(function () {
    if (tab.getAttribute("fadein") != "true") {
      window.mozRequestAnimationFrame(arguments.callee);
      return;
    }

    info(window.getComputedStyle(tab).maxWidth);
    gBrowser.removeTab(tab, {animate:true});
    ok(!tab.parentNode, "tab successfully removed");
    finish();
  });
}
