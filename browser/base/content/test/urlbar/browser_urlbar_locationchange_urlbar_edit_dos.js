"use strict";

function* checkURLBarValueStays(browser) {
  gURLBar.select();
  EventUtils.synthesizeKey("a", {});
  is(gURLBar.value, "a", "URL bar value should match after sending a key");
  yield new Promise(resolve => {
    let listener = {
      onLocationChange(aWebProgress, aRequest, aLocation, aFlags) {
        ok(aFlags & Ci.nsIWebProgressListener.LOCATION_CHANGE_SAME_DOCUMENT,
           "Should only get a same document location change");
        gBrowser.selectedBrowser.removeProgressListener(filter);
        filter = null;
        resolve();
      },
    };
    let filter = Cc["@mozilla.org/appshell/component/browser-status-filter;1"]
                     .createInstance(Ci.nsIWebProgress);
    filter.addProgressListener(listener, Ci.nsIWebProgress.NOTIFY_ALL);
    gBrowser.selectedBrowser.addProgressListener(filter);
  });
  is(gURLBar.value, "a", "URL bar should not have been changed by location changes.");
}

add_task(function*() {
  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: "http://example.com/browser/browser/base/content/test/urlbar/file_urlbar_edit_dos.html"
  }, function*(browser) {
    yield ContentTask.spawn(browser, "", function() {
      content.wrappedJSObject.dos_hash();
    });
    yield checkURLBarValueStays(browser);
    yield ContentTask.spawn(browser, "", function() {
      content.clearTimeout(content.wrappedJSObject.dos_timeout);
      content.wrappedJSObject.dos_pushState();
    });
    yield checkURLBarValueStays(browser);
  });
});

