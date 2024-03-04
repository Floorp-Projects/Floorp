// For hideSelectPopup.
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/browser/base/content/test/forms/head.js",
  this
);

function openSelectPopup(selector, win = window) {
  let popupShownPromise = BrowserTestUtils.waitForSelectPopupShown(win);
  EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true }, win);
  return popupShownPromise;
}
