add_task(function* () {
  let tab1 = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "about:robots");
  let tab2 = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "about:config");

  gBrowser.selectedTab = tab1;

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["tabs"]
    },

    background: function() {
      browser.tabs.query({
        lastFocusedWindow: true
      }, function(tabs) {
        browser.test.assertEq(tabs.length, 3, "should have three tabs");

        tabs.sort(function (tab1, tab2) { return tab1.index - tab2.index; });

        browser.test.assertEq(tabs[0].url, "about:blank", "first tab blank");
        tabs.shift();

        browser.test.assertTrue(tabs[0].active, "tab 0 active");
        browser.test.assertFalse(tabs[1].active, "tab 1 inactive");

        browser.test.assertFalse(tabs[0].pinned, "tab 0 unpinned");
        browser.test.assertFalse(tabs[1].pinned, "tab 1 unpinned");

        browser.test.assertEq(tabs[0].url, "about:robots", "tab 0 url correct");
        browser.test.assertEq(tabs[1].url, "about:config", "tab 1 url correct");

        browser.test.assertEq(tabs[0].status, "complete", "tab 0 status correct");
        browser.test.assertEq(tabs[1].status, "complete", "tab 1 status correct");

        browser.test.assertEq(tabs[0].title, "Gort! Klaatu barada nikto!", "tab 0 title correct");

        browser.test.notifyPass("tabs.query");
      });
    },
  });

  yield extension.startup();
  yield extension.awaitFinish("tabs.query");
  yield extension.unload();

  yield BrowserTestUtils.removeTab(tab1);
  yield BrowserTestUtils.removeTab(tab2);

  tab1 = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com/");
  tab2 = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.net/");
  let tab3 = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "http://test1.example.org/MochiKit/");

  // test simple queries
  extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["tabs"]
    },

    background: function() {
      browser.tabs.query({
        url: "<all_urls>"
      }, function(tabs) {
        browser.test.assertEq(tabs.length, 3, "should have three tabs");

        tabs.sort(function (tab1, tab2) { return tab1.index - tab2.index; });

        browser.test.assertEq(tabs[0].url, "http://example.com/", "tab 0 url correct");
        browser.test.assertEq(tabs[1].url, "http://example.net/", "tab 1 url correct");
        browser.test.assertEq(tabs[2].url, "http://test1.example.org/MochiKit/", "tab 2 url correct");

        browser.test.notifyPass("tabs.query");
      });
    },
  });

  yield extension.startup();
  yield extension.awaitFinish("tabs.query");
  yield extension.unload();

  // match pattern
  extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["tabs"]
    },

    background: function() {
      browser.tabs.query({
        url: "http://*/MochiKit*"
      }, function(tabs) {
        browser.test.assertEq(tabs.length, 1, "should have one tab");

        browser.test.assertEq(tabs[0].url, "http://test1.example.org/MochiKit/", "tab 0 url correct");

        browser.test.notifyPass("tabs.query");
      });
    },
  });

  yield extension.startup();
  yield extension.awaitFinish("tabs.query");
  yield extension.unload();

  // match array of patterns
  extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["tabs"]
    },

    background: function() {
      browser.tabs.query({
        url: ["http://*/MochiKit*", "http://*.com/*"]
      }, function(tabs) {
        browser.test.assertEq(tabs.length, 2, "should have two tabs");

        tabs.sort(function (tab1, tab2) { return tab1.index - tab2.index; });

        browser.test.assertEq(tabs[0].url, "http://example.com/", "tab 0 url correct");
        browser.test.assertEq(tabs[1].url, "http://test1.example.org/MochiKit/", "tab 1 url correct");

        browser.test.notifyPass("tabs.query");
      });
    },
  });

  yield extension.startup();
  yield extension.awaitFinish("tabs.query");
  yield extension.unload();

  yield BrowserTestUtils.removeTab(tab1);
  yield BrowserTestUtils.removeTab(tab2);
  yield BrowserTestUtils.removeTab(tab3);
});
