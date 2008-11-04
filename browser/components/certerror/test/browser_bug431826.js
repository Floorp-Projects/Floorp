var newBrowser

function test() {
  waitForExplicitFinish();
  
  var newTab = gBrowser.addTab();
  gBrowser.selectedTab = newTab;
  newBrowser = gBrowser.getBrowserForTab(newTab);
  
  // Navigate to a site with a broken cert
  newBrowser.contentWindow.location = 'https://nocert.example.com/';
  // XXX - This timer and the next should be replaced with an event
  // handler when bug 425001 is fixed.
  window.setTimeout(testBrokenCert, 2000);
}

function testBrokenCert() {
  
  // Confirm that we are displaying the contributed error page, not the default
  ok(/^about:certerror/.test(gBrowser.contentWindow.document.documentURI), "Broken page should go to about:certerror, not about:neterror");
  
  // Confirm that the expert section is collapsed
  var expertDiv = gBrowser.contentWindow.document.getElementById("expertContent");
  ok(expertDiv, "Expert content div should exist");
  ok(expertDiv.hasAttribute("collapsed"), "Expert content should be collapsed by default");
  
  // Tweak the expert mode pref
  Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch)
                                          .setBoolPref("browser.xul.error_pages.expert_bad_cert", true);
  
  newBrowser.reload();
  window.setTimeout(testExpertPref, 2000);
}

function testExpertPref() {
  
  var expertDiv = gBrowser.contentWindow.document.getElementById("expertContent");
  ok(!expertDiv.hasAttribute("collapsed"), "Expert content should not be collapsed with the expert mode pref set");
  
  // Clean up
  gBrowser.removeCurrentTab();
  finish();
}
