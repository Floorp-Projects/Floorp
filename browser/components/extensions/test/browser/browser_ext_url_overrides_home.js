/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */

"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "Preferences",
                                  "resource://gre/modules/Preferences.jsm");

const HOME_URI_1 = "webext-home-1.html";
const HOME_URI_2 = "webext-home-2.html";
const HOME_URI_3 = "webext-home-3.html";

add_task(function* test_multiple_extensions_overriding_newtab_page() {
  let defaultHomePage = Preferences.get("browser.startup.homepage");

  is(Preferences.get("browser.startup.homepage"), defaultHomePage,
     `Default home url should be ${defaultHomePage}`);

  let ext1 = ExtensionTestUtils.loadExtension({
    manifest: {"chrome_url_overrides": {}},
  });

  let ext2 = ExtensionTestUtils.loadExtension({
    manifest: {"chrome_url_overrides": {home: HOME_URI_1}},
  });

  let ext3 = ExtensionTestUtils.loadExtension({
    manifest: {"chrome_url_overrides": {home: HOME_URI_2}},
  });

  let ext4 = ExtensionTestUtils.loadExtension({
    manifest: {"chrome_url_overrides": {home: HOME_URI_3}},
  });

  yield ext1.startup();

  is(Preferences.get("browser.startup.homepage"), defaultHomePage,
       `Default home url should still be ${defaultHomePage}`);

  yield ext2.startup();

  ok(Preferences.get("browser.startup.homepage").endsWith(HOME_URI_1),
     "Home url should be overriden by the second extension.");

  yield ext1.unload();

  ok(Preferences.get("browser.startup.homepage").endsWith(HOME_URI_1),
     "Home url should still be overriden by the second extension.");

  yield ext3.startup();

  ok(Preferences.get("browser.startup.homepage").endsWith(HOME_URI_1),
     "Home url should still be overriden by the second extension.");

  yield ext2.unload();

  ok(Preferences.get("browser.startup.homepage").endsWith(HOME_URI_2),
     "Home url should be overriden by the third extension.");

  yield ext4.startup();

  ok(Preferences.get("browser.startup.homepage").endsWith(HOME_URI_2),
     "Home url should be overriden by the third extension.");

  yield ext4.unload();

  ok(Preferences.get("browser.startup.homepage").endsWith(HOME_URI_2),
     "Home url should be overriden by the third extension.");

  yield ext3.unload();

  is(Preferences.get("browser.startup.homepage"), defaultHomePage,
     `Home url should be reset to ${defaultHomePage}`);
});
