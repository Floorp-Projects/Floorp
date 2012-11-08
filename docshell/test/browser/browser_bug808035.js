/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test for bug 808035.
// When we open a new tab, the initial URI is the transient <about:blank> and 
// then requested URI would be loaded. When the URI's shceme is "javascript:",
// we can *replace* the transient <about:blank> with an actual requet
// through <javascript:location.replace("http://example.org/")>.
//
// There's no session history entry corresponding to the transient
// <about:blank>. But we should make sure there exists a session history entry
// for <http://example.org>.

function test() {
  const NEW_URI = "http://test1.example.org/";
  const REQUESTED_URI = "javascript:void(location.replace('" + NEW_URI +
                        "'))";

  waitForExplicitFinish();

  let tab = gBrowser.addTab(REQUESTED_URI);
  let browser = tab.linkedBrowser;

  browser.addEventListener('load', function(aEvent) {
    browser.removeEventListener('load', arguments.callee, true);

    is(browser.contentWindow.location.href, NEW_URI, "The URI is OK.");
    is(browser.contentWindow.history.length, 1, "There exists a SH entry.");

    gBrowser.removeTab(tab);
    finish();
  }, true);
}
