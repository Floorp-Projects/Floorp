/* Make sure that netError won't allow HTML injection through badcert parameters.  See bug 441169. */
var newBrowser

// An edited version of the standard neterror url which attempts to
// insert a <span id="test_span"> tag into the text.  We will navigate to this page
// and ensure that the span tag is not parsed as HTML.
var chromeURL = "about:neterror?e=nssBadCert&u=https%3A//test.kuix.de/&c=UTF-8&d=This%20sentence%20should%20not%20be%20parsed%20to%20include%20a%20%3Cspan%20id=%22test_span%22%3Enamed%3C/span%3E%20span%20tag.%0A%0AThe%20certificate%20is%20only%20valid%20for%20%3Ca%20id=%22cert_domain_link%22%20title=%22kuix.de%22%3Ekuix.de%3C/a%3E%0A%0A(Error%20code%3A%20ssl_error_bad_cert_domain)";

function test() {
  waitForExplicitFinish();
  
  var newTab = gBrowser.addTab();
  gBrowser.selectedTab = newTab;
  newBrowser = gBrowser.getBrowserForTab(newTab);
  
  window.addEventListener("DOMContentLoaded", checkPage, false);
  newBrowser.contentWindow.location = chromeURL;
}

function checkPage() {
  window.removeEventListener("DOMContentLoaded", checkPage, false);
  
  is(newBrowser.contentDocument.getElementById("test_span"), null, "Error message should not be parsed as HTML, and hence shouldn't include the 'test_span' element.");
  
  gBrowser.removeCurrentTab();
  finish();
}
