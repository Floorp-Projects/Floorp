var tab;

function test() {
  waitForExplicitFinish();

  // Force-enable tab animations
  gReduceMotionOverride = false;

  tab = BrowserTestUtils.addTab(gBrowser);
  isnot(
    tab.getAttribute("fadein"),
    "true",
    "newly opened tab is yet to fade in"
  );

  // Try to remove the tab right before the opening animation's first frame
  window.requestAnimationFrame(checkAnimationState);
}

function checkAnimationState() {
  is(tab.getAttribute("fadein"), "true", "tab opening animation initiated");

  info(window.getComputedStyle(tab).maxWidth);
  gBrowser.removeTab(tab, { animate: true });
  if (!tab.parentNode) {
    ok(
      true,
      "tab removed synchronously since the opening animation hasn't moved yet"
    );
    finish();
    return;
  }

  info(
    "tab didn't close immediately, so the tab opening animation must have started moving"
  );
  info("waiting for the tab to close asynchronously");
  tab.addEventListener(
    "TabAnimationEnd",
    function listener() {
      executeSoon(function() {
        ok(!tab.parentNode, "tab removed asynchronously");
        finish();
      });
    },
    { once: true }
  );
}
