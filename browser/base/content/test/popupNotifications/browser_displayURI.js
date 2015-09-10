/*
 * Make sure that the origin is shown for ContentPermissionPrompt
 * consumers e.g. geolocation.
*/

add_task(function* test_displayURI() {
  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: "https://test1.example.com/",
  }, function*(browser) {
    let popupShownPromise = new Promise((resolve, reject) => {
      onPopupEvent("popupshown", function() {
        resolve(this);
      });
    });
    yield ContentTask.spawn(browser, null, function*() {
      content.navigator.geolocation.getCurrentPosition(function (pos) {
        // Do nothing
      });
    });
    let panel = yield popupShownPromise;
    let notification = panel.children[0];
    let body = document.getAnonymousElementByAttribute(notification,
                                                       "class",
                                                       "popup-notification-body");
    ok(body.innerHTML.includes("example.com"), "Check that at least the eTLD+1 is present in the markup");
  });
});
