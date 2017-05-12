/*
 * Make sure that the origin is shown for ContentPermissionPrompt
 * consumers e.g. geolocation.
*/

add_task(async function test_displayURI() {
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: "https://test1.example.com/",
  }, async function(browser) {
    let popupShownPromise = waitForNotificationPanel();
    await ContentTask.spawn(browser, null, async function() {
      content.navigator.geolocation.getCurrentPosition(function(pos) {
        // Do nothing
      });
    });
    let panel = await popupShownPromise;
    let notification = panel.children[0];
    let body = document.getAnonymousElementByAttribute(notification,
                                                       "class",
                                                       "popup-notification-body");
    ok(body.innerHTML.includes("example.com"), "Check that at least the eTLD+1 is present in the markup");
  });
});
