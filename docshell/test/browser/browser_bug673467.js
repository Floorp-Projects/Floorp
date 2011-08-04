/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test for bug 673467.  In a new tab, load a page which inserts a new iframe
// before the load and then sets its location during the load.  This should
// create just one SHEntry.

var doc = "data:text/html,<html><body onload='load()'>" +
 "<script>" +
 "  var iframe = document.createElement('iframe');" +
 "  iframe.id = 'iframe';" +
 "  document.documentElement.appendChild(iframe);" +
 "  function load() {" +
 "    iframe.src = 'data:text/html,Hello!';" +
 "  }" +
 "</script>" +
 "</body></html>"

function test() {
  waitForExplicitFinish();

  let tab = gBrowser.addTab(doc);
  let tabBrowser = tab.linkedBrowser;

  tabBrowser.addEventListener('load', function(aEvent) {
    tabBrowser.removeEventListener('load', arguments.callee, true);

    // The main page has loaded.  Now wait for the iframe to load.
    let iframe = tabBrowser.contentWindow.document.getElementById('iframe');
    iframe.addEventListener('load', function(aEvent) {

      // Wait for the iframe to load the new document, not about:blank.
      if (!iframe.src)
        return;

      iframe.removeEventListener('load', arguments.callee, true);
      let shistory = tabBrowser.contentWindow
                      .QueryInterface(Ci.nsIInterfaceRequestor)
                      .getInterface(Ci.nsIWebNavigation)
                      .sessionHistory;

      is(shistory.count, 1, 'shistory count should be 1.');

      gBrowser.removeTab(tab);
      finish();

    }, true);
  }, true);
}
