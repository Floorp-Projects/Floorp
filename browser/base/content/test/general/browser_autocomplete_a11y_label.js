/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function promisePopupShown(popup) {
  if (popup.state = "open")
    return Promise.resolve();

  let deferred = Promise.defer();
  popup.addEventListener("popupshown", function onPopupShown(event) {
    popup.removeEventListener("popupshown", onPopupShown);
    deferred.resolve();
  });

  return deferred.promise;
}

function promisePopupHidden(popup) {
  if (popup.state = "closed")
    return Promise.resolve();

  let deferred = Promise.defer();
  popup.addEventListener("popuphidden", function onPopupHidden(event) {
    popup.removeEventListener("popuphidden", onPopupHidden);
    deferred.resolve();
  });

  popup.closePopup();

  return deferred.promise;
}


function* check_a11y_label(inputText, expectedLabel) {
  let searchDeferred = Promise.defer();

  let onSearchComplete = gURLBar.onSearchComplete;
  registerCleanupFunction(() => {
    gURLBar.onSearchComplete = onSearchComplete;
  });
  gURLBar.onSearchComplete = function () {
    ok(gURLBar.popupOpen, "The autocomplete popup is correctly open");
    onSearchComplete.apply(gURLBar);
    gURLBar.onSearchComplete = onSearchComplete;
    searchDeferred.resolve();
  }

  gURLBar.focus();
  gURLBar.value = inputText.slice(0, -1);
  EventUtils.synthesizeKey(inputText.slice(-1) , {});
  yield searchDeferred.promise;
  // On Linux, the popup may or may not be open at this stage. So we need
  // additional checks to ensure we wait long enough.
  yield promisePopupShown(gURLBar.popup);

  let firstResult = gURLBar.popup.richlistbox.firstChild;
  is(firstResult.getAttribute("type"), "action switchtab", "Expect right type attribute");
  is(firstResult.label, expectedLabel, "Result a11y label should be as expected");
}

add_task(function*() {
  // This test is only relevant if UnifiedComplete is enabled.
  if (!Services.prefs.getBoolPref("browser.urlbar.unifiedcomplete"))
    return;

  let tab = gBrowser.addTab("about:about");
  yield promiseTabLoaded(tab);

  yield check_a11y_label("% about", "about:about moz-action:switchtab,about:about Tab");

  yield promisePopupHidden(gURLBar.popup);
  gBrowser.removeTab(tab);
});
