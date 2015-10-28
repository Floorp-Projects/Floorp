function remote(task) {
  return ContentTask.spawn(gBrowser.selectedBrowser, null, task);
}

add_task(function* () {
  gBrowser.selectedTab = gBrowser.addTab();

  let promise = remote(function () {
    return ContentTaskUtils.waitForEvent(this, "DOMContentLoaded", true, event => {
      return content.document.documentURI != "about:blank";
    }).then(() => 0); // don't want to send the event to the chrome process
  });
  gBrowser.loadURI("https://nocert.example.com/");
  yield promise;

  let uri = yield remote(() => {
    return content.document.documentURI;
  });

  // Confirm that we are displaying the contributed error page, not the default
  ok(uri.startsWith("about:certerror"), "Broken page should go to about:certerror, not about:neterror");

  let advancedDiv, advancedDivVisibility, technicalDivCollapsed;

  [advancedDiv, advancedDivVisibility] = yield remote(() => {
    let div = content.document.getElementById("advancedPanel");
    if (div) {
      return [true, div.ownerDocument.defaultView.getComputedStyle(div, "").visibility];
    } else {
      return [null, null];
    }
  });

  // Confirm that the expert section is collapsed
  ok(advancedDiv, "Advanced content div should exist");
  is(advancedDivVisibility, "hidden", "Advanced content should not be visible by default");

  // Tweak the expert mode pref
  gPrefService.setBoolPref("browser.xul.error_pages.expert_bad_cert", true);

  promise = remote(function () {
    return ContentTaskUtils.waitForEvent(this, "DOMContentLoaded", true);
  });
  gBrowser.reload();
  yield promise;

  [advancedDiv, advancedDivVisibility] = yield remote(() => {
    let div = content.document.getElementById("advancedPanel");
    if (div) {
      return [true, div.ownerDocument.defaultView.getComputedStyle(div, "").visibility];
    } else {
      return [null, null];
    }
  });

  ok(advancedDiv, "Advanced content div should exist");
  is(advancedDivVisibility, "visible", "Advanced content should be visible by default");

  // Clean up
  gBrowser.removeCurrentTab();
  if (gPrefService.prefHasUserValue("browser.xul.error_pages.expert_bad_cert"))
    gPrefService.clearUserPref("browser.xul.error_pages.expert_bad_cert");
});
