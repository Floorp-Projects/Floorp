"use strict";

add_task(function* test_themes_disabled_by_default() {
  let manifest = {"theme": {}};
  let extension = ExtensionTestUtils.loadExtension({manifest});

  yield extension.startup();
  let enabled = yield extension.awaitMessage("themes-enabled");
  is(enabled, false, "Themes should be disabled");
  yield extension.unload();
});

add_task(function* test_themes_enabled_with_preference() {
  Services.prefs.setBoolPref("extensions.webextensions.themes.enabled", true);

  let manifest = {"theme": {}};
  let extension = ExtensionTestUtils.loadExtension({manifest});

  yield extension.startup();
  let enabled = yield extension.awaitMessage("themes-enabled");
  is(enabled, true, "Themes should be enabled");
  yield extension.unload();
});
