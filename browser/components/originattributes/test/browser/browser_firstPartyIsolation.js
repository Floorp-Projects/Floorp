const BASE_URL = "http://mochi.test:8888/browser/browser/components/originattributes/test/browser/";
const BASE_DOMAIN = "mochi.test";

add_task(function* setup() {
  Services.prefs.setBoolPref("privacy.firstparty.isolate", true);
  registerCleanupFunction(function () {
    Services.prefs.clearUserPref("privacy.firstparty.isolate");
  });
});

/**
 * Test for the top-level document and child iframes should have the
 * firstPartyDomain attribute.
 */
add_task(function* principal_test() {
  let tab = gBrowser.addTab(BASE_URL + "test_firstParty.html");
  yield BrowserTestUtils.browserLoaded(tab.linkedBrowser, true, function (url) {
    return url == BASE_URL + "test_firstParty.html";
  });

  yield ContentTask.spawn(tab.linkedBrowser, { firstPartyDomain: BASE_DOMAIN }, function* (attrs) {
    info("document principal: " + content.document.nodePrincipal.origin);
    Assert.equal(docShell.getOriginAttributes().firstPartyDomain, "",
                 "top-level docShell shouldn't have firstPartyDomain attribute.");
    Assert.equal(content.document.nodePrincipal.originAttributes.firstPartyDomain,
                 attrs.firstPartyDomain, "The document should have firstPartyDomain");

    for (let i = 1; i < 4; i++) {
      let iframe = content.document.getElementById("iframe" + i);
      info("iframe principal: " + iframe.contentDocument.nodePrincipal.origin);
      Assert.equal(iframe.frameLoader.docShell.getOriginAttributes().firstPartyDomain,
                   attrs.firstPartyDomain, "iframe's docshell should have firstPartyDomain");
      Assert.equal(iframe.contentDocument.nodePrincipal.originAttributes.firstPartyDomain,
                   attrs.firstPartyDomain, "iframe should have firstPartyDomain");
    }
  });

  gBrowser.removeTab(tab);
});

/**
 * Test for the cookie jars of the top-level document and child iframe should be
 * isolated by firstPartyDomain.
 */
add_task(function* cookie_test() {
  let tab = gBrowser.addTab(BASE_URL + "test_firstParty_cookie.html");
  yield BrowserTestUtils.browserLoaded(tab.linkedBrowser, true);

  let iter = Services.cookies.enumerator;
  let count = 0;
  while (iter.hasMoreElements()) {
    count++;
    let cookie = iter.getNext().QueryInterface(Ci.nsICookie2);
    Assert.equal(cookie.value, "foo", "Cookie value should be foo");
    Assert.equal(cookie.originAttributes.firstPartyDomain, BASE_DOMAIN, "Cookie's origin attributes should be " + BASE_DOMAIN);
  }

  // one cookie is from requesting test.js from top-level doc, and the other from
  // requesting test2.js from iframe test2.html.
  Assert.equal(count, 2, "Should have two cookies");

  gBrowser.removeTab(tab);
});

/**
 * Test for after redirect, the top-level document should update the firstPartyDomain
 * attribute. However if the redirect is happening on the iframe, the attribute
 * should remain the same.
 */
add_task(function* redirect_test() {
  let tab = gBrowser.addTab(BASE_URL + "test_firstParty_http_redirect.html");
  yield BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  yield ContentTask.spawn(tab.linkedBrowser, { firstPartyDomain: "example.com" }, function* (attrs) {
    info("document principal: " + content.document.nodePrincipal.origin);
    info("document uri: " + content.document.documentURI);

    Assert.equal(content.document.documentURI, "http://example.com/browser/browser/components/originattributes/test/browser/dummy.html",
                 "The page should have been redirected to http://example.com/browser/browser/components/originattributes/test/browser/dummy.html");
    Assert.equal(content.document.nodePrincipal.originAttributes.firstPartyDomain,
                 attrs.firstPartyDomain, "The document should have firstPartyDomain");
  });

  // Since this is a HTML redirect, we wait until the final page is loaded.
  let tab2 = gBrowser.addTab(BASE_URL + "test_firstParty_html_redirect.html");
  yield BrowserTestUtils.browserLoaded(tab2.linkedBrowser, false, function(url) {
    return url == "http://example.com/";
  });

  yield ContentTask.spawn(tab2.linkedBrowser, { firstPartyDomain: "example.com" }, function* (attrs) {
    info("2nd tab document principal: " + content.document.nodePrincipal.origin);
    info("2nd tab document uri: " + content.document.documentURI);
    Assert.equal(content.document.documentURI, "http://example.com/",
                 "The page should have been redirected to http://example.com");
    Assert.equal(content.document.nodePrincipal.originAttributes.firstPartyDomain,
                 attrs.firstPartyDomain, "The document should have firstPartyDomain");
  });

  let tab3 = gBrowser.addTab(BASE_URL + "test_firstParty_iframe_http_redirect.html");
  yield BrowserTestUtils.browserLoaded(tab3.linkedBrowser, true, function(url) {
    return url == (BASE_URL + "test_firstParty_iframe_http_redirect.html");
  });

  // This redirect happens on the iframe, so unlike the two redirect tests above,
  // the firstPartyDomain should still stick to the current top-level document,
  // which is mochi.test.
  yield ContentTask.spawn(tab3.linkedBrowser, { firstPartyDomain: "mochi.test" }, function* (attrs) {
    let iframe = content.document.getElementById("iframe1");
    info("iframe document principal: " + iframe.contentDocument.nodePrincipal.origin);
    info("iframe document uri: " + iframe.contentDocument.documentURI);

    Assert.equal(iframe.contentDocument.documentURI, "http://example.com/browser/browser/components/originattributes/test/browser/dummy.html",
                 "The page should have been redirected to http://example.com/browser/browser/components/originattributes/test/browser/dummy.html");
    Assert.equal(iframe.contentDocument.nodePrincipal.originAttributes.firstPartyDomain,
                 attrs.firstPartyDomain, "The iframe should have firstPartyDomain: " + attrs.firstPartyDomain);
  });

  gBrowser.removeTab(tab);
  gBrowser.removeTab(tab2);
  gBrowser.removeTab(tab3);
});

/**
 * Test for postMessage between document and iframe.
 */
add_task(function* postMessage_test() {
  let tab = gBrowser.addTab(BASE_URL + "test_firstParty_postMessage.html");

  // The top-level page will post a message to its child iframe, and wait for
  // another message from the iframe, once it receives the message, it will
  // create another iframe, dummy.html.
  // So we wait until dummy.html is loaded
  yield BrowserTestUtils.browserLoaded(tab.linkedBrowser, true, function (url) {
    return url == BASE_URL + "dummy.html";
  });

  yield ContentTask.spawn(tab.linkedBrowser, {}, function* () {
    info("document principal: " + content.document.nodePrincipal.origin);
    let value = content.document.getElementById("message").textContent;
    Assert.equal(value, "OK");
  });

  gBrowser.removeTab(tab);
});

/**
 * When the web page calls window.open, the new window should have the same
 * firstPartyDomain attribute.
 */
add_task(function* openWindow_test() {
  Services.prefs.setIntPref("browser.link.open_newwindow", 2);
  registerCleanupFunction(function () {
    Services.prefs.clearUserPref("browser.link.open_newwindow");
  });

  let tab = gBrowser.addTab(BASE_URL + "window.html");
  let win = yield BrowserTestUtils.waitForNewWindow();

  yield ContentTask.spawn(win.gBrowser.selectedBrowser, { firstPartyDomain: "mochi.test" }, function* (attrs) {
    Assert.equal(docShell.getOriginAttributes().firstPartyDomain, attrs.firstPartyDomain,
                 "window.open() should have firstPartyDomain attribute");
    Assert.equal(content.document.nodePrincipal.originAttributes.firstPartyDomain,
                 attrs.firstPartyDomain, "The document should have firstPartyDomain");

    let iframe = content.document.getElementById("iframe1");
    Assert.equal(iframe.frameLoader.docShell.getOriginAttributes().firstPartyDomain,
                 attrs.firstPartyDomain, "iframe's docshell should have firstPartyDomain");
    Assert.equal(iframe.contentDocument.nodePrincipal.originAttributes.firstPartyDomain,
                 attrs.firstPartyDomain, "iframe should have firstPartyDomain");
  });

  gBrowser.removeTab(tab);
  yield BrowserTestUtils.closeWindow(win);
});

