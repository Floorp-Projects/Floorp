function test() {
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();

  // Navigate to a site with a broken cert
  window.addEventListener("DOMContentLoaded", testBrokenCert, true);
  content.location = "https://nocert.example.com/";
}

function testBrokenCert() {
  if (gBrowser.contentDocument.documentURI === "about:blank")
    return;
  window.removeEventListener("DOMContentLoaded", testBrokenCert, true);

  // Confirm that we are displaying the contributed error page, not the default
  ok(gBrowser.contentDocument.documentURI.startsWith("about:certerror"), "Broken page should go to about:certerror, not about:neterror");

  // Confirm that the expert section is collapsed
  var advancedDiv = gBrowser.contentDocument.getElementById("advancedPanel");
  ok(advancedDiv, "Advanced content div should exist");
  is_element_hidden(advancedDiv, "Advanced content should not be visible by default");

  // Tweak the expert mode pref
  gPrefService.setBoolPref("browser.xul.error_pages.expert_bad_cert", true);

  window.addEventListener("DOMContentLoaded", testExpertPref, true);
  gBrowser.reload();
}

function testExpertPref() {
  window.removeEventListener("DOMContentLoaded", testExpertPref, true);
  var advancedDiv = gBrowser.contentDocument.getElementById("advancedPanel");
  ok(advancedDiv, "Advanced content div should exist");
  is_element_visible(advancedDiv, "Advanced content should be visible by default");

  // Clean up
  gBrowser.removeCurrentTab();
  if (gPrefService.prefHasUserValue("browser.xul.error_pages.expert_bad_cert"))
    gPrefService.clearUserPref("browser.xul.error_pages.expert_bad_cert");
  finish();
}
