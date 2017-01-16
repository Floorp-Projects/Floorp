"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "Preferences",
                                  "resource://gre/modules/Preferences.jsm");

/* eslint-disable mozilla/balanced-listeners */
extensions.on("manifest_theme", (type, directive, extension, manifest) => {
  let enabled = Preferences.get("extensions.webextensions.themes.enabled");
  extension.emit("test-message", "themes-enabled", enabled);

  if (enabled) {
    // Apply theme only if themes are enabled.
  }
});

extensions.on("shutdown", (type, extension) => {
  // Remove theme only if it has been applied.
});
/* eslint-enable mozilla/balanced-listeners */
