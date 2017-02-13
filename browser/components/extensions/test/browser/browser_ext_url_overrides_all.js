/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */

"use strict";

XPCOMUtils.defineLazyServiceGetter(this, "aboutNewTabService",
                                   "@mozilla.org/browser/aboutnewtab-service;1",
                                   "nsIAboutNewTabService");

const NEWTAB_URI = "webext-newtab.html";
const HOME_URI = "webext-home.html";

add_task(function* test_extensions_overriding_different_pages() {
  let defaultHomePage = Preferences.get("browser.startup.homepage");
  let defaultNewtabPage = aboutNewTabService.newTabURL;

  is(Preferences.get("browser.startup.homepage"), defaultHomePage,
    `Default home url should be ${defaultHomePage}`);
  is(aboutNewTabService.newTabURL, defaultNewtabPage,
    `Default newtab url should be ${defaultNewtabPage}`);

  let ext1 = ExtensionTestUtils.loadExtension({
    manifest: {"chrome_url_overrides": {}},
  });

  let ext2 = ExtensionTestUtils.loadExtension({
    manifest: {"chrome_url_overrides": {newtab: NEWTAB_URI}},
  });

  let ext3 = ExtensionTestUtils.loadExtension({
    manifest: {"chrome_url_overrides": {home: HOME_URI}},
  });

  yield ext1.startup();

  is(aboutNewTabService.newTabURL, defaultNewtabPage,
    `Default newtab url should still be ${defaultNewtabPage}`);
  is(Preferences.get("browser.startup.homepage"), defaultHomePage,
    `Default home url should be ${defaultHomePage}`);

  yield ext2.startup();

  ok(aboutNewTabService.newTabURL.endsWith(NEWTAB_URI),
    "Newtab url should be overriden by the second extension.");
  is(Preferences.get("browser.startup.homepage"), defaultHomePage,
    `Default home url should be ${defaultHomePage}`);

  yield ext1.unload();

  ok(aboutNewTabService.newTabURL.endsWith(NEWTAB_URI),
    "Newtab url should still be overriden by the second extension.");
  is(Preferences.get("browser.startup.homepage"), defaultHomePage,
    `Default home url should be ${defaultHomePage}`);

  yield ext3.startup();

  ok(aboutNewTabService.newTabURL.endsWith(NEWTAB_URI),
    "Newtab url should still be overriden by the second extension.");
  ok(Preferences.get("browser.startup.homepage").endsWith(HOME_URI),
    "Home url should be overriden by the third extension.");

  yield ext2.unload();

  is(aboutNewTabService.newTabURL, defaultNewtabPage,
    `Newtab url should be reset to ${defaultNewtabPage}`);
  ok(Preferences.get("browser.startup.homepage").endsWith(HOME_URI),
    "Home url should still be overriden by the third extension.");

  yield ext3.unload();

  is(aboutNewTabService.newTabURL, defaultNewtabPage,
    `Newtab url should be reset to ${defaultNewtabPage}`);
  is(Preferences.get("browser.startup.homepage"), defaultHomePage,
    `Home url should be reset to ${defaultHomePage}`);
});

add_task(function* test_extensions_with_multiple_overrides() {
  let ext = ExtensionTestUtils.loadExtension({
    manifest: {"chrome_url_overrides": {
      newtab: NEWTAB_URI,
      home: HOME_URI,
    }},
  });

  SimpleTest.waitForExplicitFinish();
  let waitForConsole = new Promise(resolve => {
    SimpleTest.monitorConsole(resolve, [{
      message: /Extensions can override only one page./,
    }]);
  });

  yield ext.startup();
  yield ext.unload();

  SimpleTest.endMonitorConsole();
  yield waitForConsole;
});
