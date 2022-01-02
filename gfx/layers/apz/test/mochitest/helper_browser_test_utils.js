// For hideSelectPopup.
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/browser/base/content/test/forms/head.js",
  this
);

function openSelectPopup(selectPopup, selector = "select", win = window) {
  let popupShownPromise = BrowserTestUtils.waitForEvent(
    selectPopup,
    "popupshown"
  );

  EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true }, win);
  return popupShownPromise;
}
