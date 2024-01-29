/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_setup(async function () {
  registerCleanupFunction(async function () {
    gURLBar.handleRevert();
    await PlacesUtils.history.clear();
  });
  SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.autoFill", true],
      ["browser.urlbar.suggest.quickactions", false],
    ],
  });

  await PlacesUtils.history.clear();
  await PlacesTestUtils.addVisits([
    "https://example.com/",
    "https://example.com/foo",
  ]);
});

add_task(async function () {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "exam",
  });
  Assert.equal(
    gURLBar.value,
    "example.com/",
    "autofilled value is as expected"
  );

  await UrlbarTestUtils.promisePopupClose(window);

  goDoCommand("cmd_selectAll");
  goDoCommand("cmd_copy");
  goDoCommand("cmd_paste");
  Assert.equal(
    gURLBar.inputField.value,
    "https://example.com/",
    "pasted value contains scheme"
  );
});
