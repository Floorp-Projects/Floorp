/* Check for the intended visibility of the "Ignore this warning" text*/
var newBrowser

function test() {
  waitForExplicitFinish();
  
  var newTab = gBrowser.addTab();
  gBrowser.selectedTab = newTab;
  newBrowser = gBrowser.getBrowserForTab(newTab);
  
  // Navigate to malware site.  Can't use an onload listener here since
  // error pages don't fire onload
  window.addEventListener("DOMContentLoaded", testMalware, true);
  newBrowser.contentWindow.location = 'http://www.mozilla.com/firefox/its-an-attack.html';
}

function testMalware() {
  window.removeEventListener("DOMContentLoaded", testMalware, true);

  // Confirm that "Ignore this warning" is visible - bug 422410
  var el = newBrowser.contentDocument.getElementById("ignoreWarningButton");
  ok(el, "Ignore warning button should be present for malware");
  
  var style = newBrowser.contentWindow.getComputedStyle(el, null);
  is(style.display, "-moz-box", "Ignore Warning button should be display:-moz-box for malware");
  
  // Now launch the phishing test
  window.addEventListener("DOMContentLoaded", testPhishing, true);
  newBrowser.contentWindow.location = 'http://www.mozilla.com/firefox/its-a-trap.html';
}

function testPhishing() {
  window.removeEventListener("DOMContentLoaded", testPhishing, true);
  
  var el = newBrowser.contentDocument.getElementById("ignoreWarningButton");
  ok(el, "Ignore warning button should be present for phishing");
  
  var style = newBrowser.contentWindow.getComputedStyle(el, null);
  is(style.display, "-moz-box", "Ignore Warning button should be display:-moz-box for phishing");
  
  gBrowser.removeCurrentTab();
  finish();
}
