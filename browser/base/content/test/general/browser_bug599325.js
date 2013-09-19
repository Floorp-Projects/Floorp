function test() {
  waitForExplicitFinish();

  let addonBar = document.getElementById("addon-bar");
  ok(addonBar, "got addon bar");
  ok(!isElementVisible(addonBar), "addon bar initially hidden");

  openToolbarCustomizationUI(function () {
    ok(isElementVisible(addonBar),
       "add-on bar is visible during toolbar customization");

    closeToolbarCustomizationUI(onClose);
  });

  function onClose() {
    ok(!isElementVisible(addonBar),
       "addon bar is hidden after toolbar customization");

    finish();
  }
}
