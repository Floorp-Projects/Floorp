var tab;
var performedTest = false;

function test() {
  waitForExplicitFinish();

  tab = gBrowser.addTab();
  isnot(tab.getAttribute("fadein"), "true", "newly opened tab is yet to fade in");

  // Remove the tab right before the opening animation's first frame
  window.mozRequestAnimationFrame(checkAnimationState);
  executeSoon(checkAnimationState);
}

function checkAnimationState() {
  if (performedTest)
    return;

  if (tab.getAttribute("fadein") != "true") {
    window.mozRequestAnimationFrame(checkAnimationState);
    executeSoon(checkAnimationState);
    return;
  }

  performedTest = true;

  info(window.getComputedStyle(tab).maxWidth);
  gBrowser.removeTab(tab, {animate:true});
  ok(!tab.parentNode, "tab successfully removed");
  finish();
}
