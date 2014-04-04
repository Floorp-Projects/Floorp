function test() {
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();

  // Navigate to a site with a broken cert
  window.addEventListener("DOMContentLoaded", testBrokenCert, true);
  content.location = "https://nocert.example.com/";
}

function testBrokenCert() {
  window.removeEventListener("DOMContentLoaded", testBrokenCert, true);

  // Confirm that we are displaying the contributed error page, not the default
  ok(gBrowser.contentDocument.documentURI.startsWith("about:certerror"), "Broken page should go to about:certerror, not about:neterror");

  // Confirm that the expert section is collapsed
  var expertDiv = gBrowser.contentDocument.getElementById("expertContent");
  ok(expertDiv, "Expert content div should exist");
  ok(expertDiv.hasAttribute("collapsed"), "Expert content should be collapsed by default");

  // Tweak the expert mode pref
  gPrefService.setBoolPref("browser.xul.error_pages.expert_bad_cert", true);

  window.addEventListener("DOMContentLoaded", testExpertPref, true);
  gBrowser.reload();
}

function testExpertPref() {
  window.removeEventListener("DOMContentLoaded", testExpertPref, true);
  var expertDiv = gBrowser.contentDocument.getElementById("expertContent");
  var technicalDiv = gBrowser.contentDocument.getElementById("technicalContent");
  ok(!expertDiv.hasAttribute("collapsed"), "Expert content should not be collapsed with the expert mode pref set");
  ok(!technicalDiv.hasAttribute("collapsed"), "Technical content should not be collapsed with the expert mode pref set");

  // Clean up
  gBrowser.removeCurrentTab();
  if (gPrefService.prefHasUserValue("browser.xul.error_pages.expert_bad_cert"))
    gPrefService.clearUserPref("browser.xul.error_pages.expert_bad_cert");
  finish();
}
