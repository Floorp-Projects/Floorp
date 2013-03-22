/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function test() {
  waitForExplicitFinish();
  gBrowser.selectedTab = gBrowser.addTab();
  // Open a html page with about:certerror in an iframe
  window.content.addEventListener("load", testIframeCert, true);
  content.location = "data:text/html,<iframe width='700' height='700' src='about:certerror'></iframe>";
}

function testIframeCert() {
  window.content.removeEventListener("load", testIframeCert, true);
  // Confirm that the expert section is hidden
  var doc = gBrowser.contentDocument.getElementsByTagName('iframe')[0].contentDocument;
  var eC = doc.getElementById("expertContent");
  ok(eC, "Expert content should exist")
  ok(eC.hasAttribute("hidden"), "Expert content should be hidded by default");

  // Clean up
  gBrowser.removeCurrentTab();
  
  finish();
}
