// Tests that the DOMDocElementInserted event is visible on the frame
add_task(async function () {
  let tab = BrowserTestUtils.addTab(gBrowser);
  let uri = "data:text/html;charset=utf-8,<html/>";

  let eventPromise = ContentTask.spawn(tab.linkedBrowser, null, function () {
    return new Promise(resolve => {
      addEventListener(
        "DOMDocElementInserted",
        event => resolve(event.target.documentURIObject.spec),
        {
          once: true,
        }
      );
    });
  });

  BrowserTestUtils.startLoadingURIString(tab.linkedBrowser, uri);
  let loadedURI = await eventPromise;
  is(loadedURI, uri, "Should have seen the event for the right URI");

  gBrowser.removeTab(tab);
});
