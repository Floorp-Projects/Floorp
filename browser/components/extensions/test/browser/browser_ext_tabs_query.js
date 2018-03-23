/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

requestLongerTimeout(2);

add_task(async function() {
  let tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:robots");
  let tab2 = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:config");

  gBrowser.selectedTab = tab1;

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["tabs"],
    },

    background: function() {
      browser.tabs.query({
        lastFocusedWindow: true,
      }, function(tabs) {
        browser.test.assertEq(tabs.length, 3, "should have three tabs");

        tabs.sort((tab1, tab2) => tab1.index - tab2.index);

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

        browser.test.assertThrows(
          () => browser.tabs.query({index: -1}),
          /-1 is too small \(must be at least 0\)/,
          "tab indices must be non-negative");

        browser.test.notifyPass("tabs.query");
      });
    },
  });

  await extension.startup();
  await extension.awaitFinish("tabs.query");
  await extension.unload();

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);

  tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com/");
  tab2 = await BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.net/");
  let tab3 = await BrowserTestUtils.openNewForegroundTab(gBrowser, "http://test1.example.org/MochiKit/");

  // test simple queries
  extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["tabs"],
    },

    background: function() {
      browser.tabs.query({
        url: "<all_urls>",
      }, function(tabs) {
        browser.test.assertEq(tabs.length, 3, "should have three tabs");

        tabs.sort((tab1, tab2) => tab1.index - tab2.index);

        browser.test.assertEq(tabs[0].url, "http://example.com/", "tab 0 url correct");
        browser.test.assertEq(tabs[1].url, "http://example.net/", "tab 1 url correct");
        browser.test.assertEq(tabs[2].url, "http://test1.example.org/MochiKit/", "tab 2 url correct");

        browser.test.notifyPass("tabs.query");
      });
    },
  });

  await extension.startup();
  await extension.awaitFinish("tabs.query");
  await extension.unload();

  // match pattern
  extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["tabs"],
    },

    background: function() {
      browser.tabs.query({
        url: "http://*/MochiKit*",
      }, function(tabs) {
        browser.test.assertEq(tabs.length, 1, "should have one tab");

        browser.test.assertEq(tabs[0].url, "http://test1.example.org/MochiKit/", "tab 0 url correct");

        browser.test.notifyPass("tabs.query");
      });
    },
  });

  await extension.startup();
  await extension.awaitFinish("tabs.query");
  await extension.unload();

  // match array of patterns
  extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["tabs"],
    },

    background: function() {
      browser.tabs.query({
        url: ["http://*/MochiKit*", "http://*.com/*"],
      }, function(tabs) {
        browser.test.assertEq(tabs.length, 2, "should have two tabs");

        tabs.sort((tab1, tab2) => tab1.index - tab2.index);

        browser.test.assertEq(tabs[0].url, "http://example.com/", "tab 0 url correct");
        browser.test.assertEq(tabs[1].url, "http://test1.example.org/MochiKit/", "tab 1 url correct");

        browser.test.notifyPass("tabs.query");
      });
    },
  });

  await extension.startup();
  await extension.awaitFinish("tabs.query");
  await extension.unload();

  // match title pattern
  extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["tabs"],
    },

    async background() {
      let tabs = await browser.tabs.query({
        title: "mochitest index /",
      });

      browser.test.assertEq(tabs.length, 2, "should have two tabs");

      tabs.sort((tab1, tab2) => tab1.index - tab2.index);

      browser.test.assertEq(tabs[0].title, "mochitest index /", "tab 0 title correct");
      browser.test.assertEq(tabs[1].title, "mochitest index /", "tab 1 title correct");

      tabs = await browser.tabs.query({
        title: "?ochitest index /*",
      });

      browser.test.assertEq(tabs.length, 3, "should have three tabs");

      tabs.sort((tab1, tab2) => tab1.index - tab2.index);

      browser.test.assertEq(tabs[0].title, "mochitest index /", "tab 0 title correct");
      browser.test.assertEq(tabs[1].title, "mochitest index /", "tab 1 title correct");
      browser.test.assertEq(tabs[2].title, "mochitest index /MochiKit/", "tab 2 title correct");

      browser.test.notifyPass("tabs.query");
    },
  });

  await extension.startup();
  await extension.awaitFinish("tabs.query");
  await extension.unload();

  // match highlighted
  extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["tabs"],
    },

    background: async function() {
      let tabs1 = await browser.tabs.query({highlighted: false});
      browser.test.assertEq(3, tabs1.length, "should have three non-highlighted tabs");

      let tabs2 = await browser.tabs.query({highlighted: true});
      browser.test.assertEq(1, tabs2.length, "should have one highlighted tab");

      for (let tab of [...tabs1, ...tabs2]) {
        browser.test.assertEq(tab.active, tab.highlighted, "highlighted and active are equal in tab " + tab.index);
      }

      browser.test.notifyPass("tabs.query");
    },
  });

  await extension.startup();
  await extension.awaitFinish("tabs.query");
  await extension.unload();

  // test width and height
  extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["tabs"],
    },

    background: function() {
      browser.test.onMessage.addListener(async msg => {
        let tabs = await browser.tabs.query({active: true});

        browser.test.assertEq(tabs.length, 1, "should have one tab");
        browser.test.sendMessage("dims", {width: tabs[0].width, height: tabs[0].height});
      });
      browser.test.sendMessage("ready");
    },
  });

  const RESOLUTION_PREF = "layout.css.devPixelsPerPx";
  registerCleanupFunction(() => {
    SpecialPowers.clearUserPref(RESOLUTION_PREF);
  });

  await Promise.all([extension.startup(), extension.awaitMessage("ready")]);

  for (let resolution of [2, 1]) {
    SpecialPowers.setCharPref(RESOLUTION_PREF, String(resolution));
    is(window.devicePixelRatio, resolution, "window has the required resolution");

    let {clientHeight, clientWidth} = gBrowser.selectedBrowser;

    extension.sendMessage("check-size");
    let dims = await extension.awaitMessage("dims");
    is(dims.width, clientWidth, "tab reports expected width");
    is(dims.height, clientHeight, "tab reports expected height");
  }

  await extension.unload();

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
  BrowserTestUtils.removeTab(tab3);
  SpecialPowers.clearUserPref(RESOLUTION_PREF);
});

add_task(async function testQueryPermissions() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": [],
    },

    async background() {
      try {
        let tabs = await browser.tabs.query({currentWindow: true, active: true});
        browser.test.assertEq(tabs.length, 1, "Expect query to return tabs");
        browser.test.notifyPass("queryPermissions");
      } catch (e) {
        browser.test.notifyFail("queryPermissions");
      }
    },
  });

  await extension.startup();

  await extension.awaitFinish("queryPermissions");

  await extension.unload();
});

add_task(async function testQueryWithoutURLOrTitlePermissions() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": [],
    },

    async background() {
      await browser.test.assertRejects(
        browser.tabs.query({"url": "http://www.bbc.com/"}),
        'The "tabs" permission is required to use the query API with the "url" or "title" parameters',
        "Expected tabs.query with 'url' or 'title' to fail with permissions error message");

      await browser.test.assertRejects(
        browser.tabs.query({"title": "Foo"}),
        'The "tabs" permission is required to use the query API with the "url" or "title" parameters',
        "Expected tabs.query with 'url' or 'title' to fail with permissions error message");

      browser.test.notifyPass("testQueryWithoutURLOrTitlePermissions");
    },
  });

  await extension.startup();

  await extension.awaitFinish("testQueryWithoutURLOrTitlePermissions");

  await extension.unload();
});
