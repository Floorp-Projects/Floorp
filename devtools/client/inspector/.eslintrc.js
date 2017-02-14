"use strict";

module.exports = {
  // Extend from the devtools eslintrc.
  "extends": "../../.eslintrc.js",

  "rules": {
    // The inspector is being migrated to HTML and cleaned of
    // chrome-privileged code, so this rule disallows requiring chrome
    // code. Some files in the inspector disable this rule still. The
    // goal is to enable the rule globally on all files.
    /* eslint-disable max-len */
    "mozilla/reject-some-requires": ["error",
      "^(chrome|chrome:.*|resource:.*|devtools/server/.*|.*\\.jsm|devtools/shared/platform/(chome|content)/.*)$"],

    // The webpack bundle does not support trailing commas in functions, so disabled it
    // explicitly with linting here.
    "comma-dangle": ["error", {
      "arrays": "ignore",
      "objects": "ignore",
      "imports": "ignore",
      "exports": "ignore",
      "functions": "never",
    }]
  },
};
