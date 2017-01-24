"use strict";

Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Preferences",
                                  "resource://gre/modules/Preferences.jsm");

/* eslint-disable mozilla/balanced-listeners */
extensions.on("manifest_theme", (type, directive, extension, manifest) => {
  let enabled = Preferences.get("extensions.webextensions.themes.enabled");

  if (!enabled || !manifest || !manifest.theme) {
    return;
  }
  // Apply theme only if themes are enabled.
  let lwtStyles = {footerURL: ""};
  if (manifest.theme.colors) {
    let colors = manifest.theme.colors;
    for (let color of Object.getOwnPropertyNames(colors)) {
      let val = colors[color];
      // Since values are optional, they may be `null`.
      if (val === null) {
        continue;
      }

      if (color == "accentcolor") {
        lwtStyles.accentcolor = val;
        continue;
      }
      if (color == "textcolor") {
        lwtStyles.textcolor = val;
      }
    }
  }

  if (manifest.theme.images) {
    let images = manifest.theme.images;
    for (let image of Object.getOwnPropertyNames(images)) {
      let val = images[image];
      if (val === null) {
        continue;
      }

      if (image == "headerURL") {
        lwtStyles.headerURL = val;
      }
    }
  }

  if (lwtStyles.headerURL &&
      lwtStyles.accentcolor &&
      lwtStyles.textcolor) {
    Services.obs.notifyObservers(null,
      "lightweight-theme-styling-update",
      JSON.stringify(lwtStyles));
  }
});

extensions.on("shutdown", (type, extension) => {
  Services.obs.notifyObservers(null, "lightweight-theme-styling-update", null);
});
/* eslint-enable mozilla/balanced-listeners */
