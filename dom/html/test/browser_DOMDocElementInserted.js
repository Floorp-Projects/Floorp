// Tests that the DOMDocElementInserted event is visible on the frame
add_task(function*() {
  let tab = BrowserTestUtils.addTab(gBrowser);
  let uri = "data:text/html;charset=utf-8,<html/>"

  let eventPromise = ContentTask.spawn(tab.linkedBrowser, null, function() {
    Cu.import("resource://gre/modules/PromiseUtils.jsm");
    let deferred = PromiseUtils.defer();

    let listener = (event) => {
      removeEventListener("DOMDocElementInserted", listener, true);
      deferred.resolve(event.target.documentURIObject.spec);
    };
    addEventListener("DOMDocElementInserted", listener, true);

    return deferred.promise;
  });

  tab.linkedBrowser.loadURI(uri);
  let loadedURI = yield eventPromise;
  is(loadedURI, uri, "Should have seen the event for the right URI");

  gBrowser.removeTab(tab);
});
