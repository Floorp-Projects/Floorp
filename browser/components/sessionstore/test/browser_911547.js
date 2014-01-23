/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// This tests that session restore component does restore the right content
// security policy with the document.
// The policy being tested disallows inline scripts

function test() {
  TestRunner.run();
}

function runTests() {
  // create a tab that has a CSP
  let testURL = "http://mochi.test:8888/browser/browser/components/sessionstore/test/browser_911547_sample.html";
  let tab = gBrowser.selectedTab = gBrowser.addTab(testURL);
  gBrowser.selectedTab = tab;

  let browser = tab.linkedBrowser;
  yield waitForLoad(browser);

  // this is a baseline to ensure CSP is active
  // attempt to inject and run a script via inline (pre-restore, allowed)
  injectInlineScript(browser,'document.getElementById("test_id").value = "fail";');
  is(browser.contentDocument.getElementById("test_id").value, "ok",
     "CSP should block the inline script that modifies test_id");

  // attempt to click a link to a data: URI (will inherit the CSP of the
  // origin document) and navigate to the data URI in the link.
  browser.contentDocument.getElementById("test_data_link").click();
  yield waitForLoad(browser);

  is(browser.contentDocument.getElementById("test_id2").value, "ok",
     "CSP should block the script loaded by the clicked data URI");

  // close the tab
  gBrowser.removeTab(tab);

  // open new tab and recover the state
  tab = ss.undoCloseTab(window, 0);
  yield waitForTabRestored(tab);
  browser = tab.linkedBrowser;

  is(browser.contentDocument.getElementById("test_id2").value, "ok",
     "CSP should block the script loaded by the clicked data URI after restore");

  // clean up
  gBrowser.removeTab(tab);
}

function waitForLoad(aElement) {
  aElement.addEventListener("load", function onLoad() {
    aElement.removeEventListener("load", onLoad, true);
    executeSoon(next);
  }, true);
}

function waitForTabRestored(aElement) {
  aElement.addEventListener("SSTabRestored", function tabRestored(e) {
    aElement.removeEventListener("SSTabRestored", tabRestored, true);
    executeSoon(next);
  }, true);
}

// injects an inline script element (with a text body)
function injectInlineScript(browser, scriptText) {
  let scriptElt = browser.contentDocument.createElement("script");
  scriptElt.type = 'text/javascript';
  scriptElt.text = scriptText;
  browser.contentDocument.body.appendChild(scriptElt);
}
