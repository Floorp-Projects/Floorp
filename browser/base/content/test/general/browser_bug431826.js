function remote(task) {
  return ContentTask.spawn(gBrowser.selectedBrowser, null, task);
}

add_task(function* () {
  gBrowser.selectedTab = gBrowser.addTab();

  let promise = remote(function() {
    return ContentTaskUtils.waitForEvent(this, "DOMContentLoaded", true, event => {
      return content.document.documentURI != "about:blank";
    }).then(() => 0); // don't want to send the event to the chrome process
  });
  gBrowser.loadURI("https://nocert.example.com/");
  yield promise;

  yield remote(() => {
    // Confirm that we are displaying the contributed error page, not the default
    let uri = content.document.documentURI;
    Assert.ok(uri.startsWith("about:certerror"), "Broken page should go to about:certerror, not about:neterror");
  });

  yield remote(() => {
    let div = content.document.getElementById("badCertAdvancedPanel");
    // Confirm that the expert section is collapsed
    Assert.ok(div, "Advanced content div should exist");
    Assert.equal(div.ownerGlobal.getComputedStyle(div).display,
      "none", "Advanced content should not be visible by default");
  });

  // Tweak the expert mode pref
  gPrefService.setBoolPref("browser.xul.error_pages.expert_bad_cert", true);

  promise = remote(function() {
    return ContentTaskUtils.waitForEvent(this, "DOMContentLoaded", true);
  });
  gBrowser.reload();
  yield promise;

  yield remote(() => {
    let div = content.document.getElementById("badCertAdvancedPanel");
    Assert.ok(div, "Advanced content div should exist");
    Assert.equal(div.ownerGlobal.getComputedStyle(div).display,
      "block", "Advanced content should be visible by default");
  });

  // Clean up
  gBrowser.removeCurrentTab();
  if (gPrefService.prefHasUserValue("browser.xul.error_pages.expert_bad_cert"))
    gPrefService.clearUserPref("browser.xul.error_pages.expert_bad_cert");
});
