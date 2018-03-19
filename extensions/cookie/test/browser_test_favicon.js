// tests third party cookie blocking using a favicon load directly from chrome.
// in this case, the docshell of the channel is chrome, not content; thus
// the cookie should be considered third party.

add_task(async function() {
  const iconUrl = "http://example.org/browser/extensions/cookie/test/damonbowling.jpg";
	await SpecialPowers.pushPrefEnv({"set": [["network.cookie.cookieBehavior", 1]]});

  let promise = TestUtils.topicObserved("cookie-rejected", subject => {
    let uri = subject.QueryInterface(Ci.nsIURI);
    return uri.spec == iconUrl;
  });

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com/");
  registerCleanupFunction(async function() {
    BrowserTestUtils.removeTab(tab);
  });

  // Kick off a favicon load.
  gBrowser.setIcon(tab, iconUrl);
  await promise;
  ok(true, "foreign favicon cookie was blocked");
});
