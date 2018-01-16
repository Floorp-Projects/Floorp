"use strict";

module.exports = {
  // New rules and configurations should generally be added in
  // tools/lint/eslint/eslint-plugin-mozilla/lib/configs/recommended.js to
  // allow external repositories that use the plugin to pick them up as well.
  "extends": [
    "plugin:mozilla/recommended"
  ],
  "plugins": [
    "mozilla"
  ],
  // The html plugin is enabled via a command line option on eslint. To avoid
  // bad interactions with the xml preprocessor in eslint-plugin-mozilla, we
  // turn off processing of the html plugin for .xml files.
  "settings": {
    "html/xml-extensions": [ ".xhtml" ]
  },

  "overrides": [{
    // eslint-plugin-html handles eol-last slightly different - it applies to
    // each set of script tags, so we turn it off here.
    "files": "**/*.*html",
    "rules": {
      "eol-last": "off",
    }
  }, {
    // XXX Bug 1421969. These files/directories are still being fixed,
    // so turn off mozilla/use-services for them for now.
    "files": [
      // Browser: Bug 1421379
      "browser/extensions/shield-recipe-client/test/browser/head.js",
      "browser/modules/offlineAppCache.jsm",
      "devtools/**",
      "extensions/pref/**",
      "mobile/android/**",
      "testing/**",
    ],
    "rules": {
      "mozilla/use-services": "off",
    }
  }]
};
