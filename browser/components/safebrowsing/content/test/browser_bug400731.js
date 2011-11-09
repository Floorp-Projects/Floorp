/* Check for the intended visibility of the "Ignore this warning" text*/

function test() {
  waitForExplicitFinish();
  
  gBrowser.selectedTab = gBrowser.addTab();
  
  // Navigate to malware site.  Can't use an onload listener here since
  // error pages don't fire onload
  window.addEventListener("DOMContentLoaded", testMalware, true);
  content.location = "http://www.mozilla.org/firefox/its-an-attack.html";
}

function testMalware() {
  window.removeEventListener("DOMContentLoaded", testMalware, true);

  // Confirm that "Ignore this warning" is visible - bug 422410
  var el = content.document.getElementById("ignoreWarningButton");
  ok(el, "Ignore warning button should be present for malware");
  
  var style = content.getComputedStyle(el, null);
  is(style.display, "inline", "Ignore Warning button should be display:inline for malware");
  
  // Now launch the phishing test
  window.addEventListener("DOMContentLoaded", testPhishing, true);
  content.location = "http://www.mozilla.org/firefox/its-a-trap.html";
}

function testPhishing() {
  window.removeEventListener("DOMContentLoaded", testPhishing, true);
  
  var el = content.document.getElementById("ignoreWarningButton");
  ok(el, "Ignore warning button should be present for phishing");
  
  var style = content.getComputedStyle(el, null);
  is(style.display, "inline", "Ignore Warning button should be display:inline for phishing");
  
  gBrowser.removeCurrentTab();
  finish();
}
