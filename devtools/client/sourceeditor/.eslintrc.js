"use strict";

module.exports = {
  // Extend from the devtools eslintrc.
  "extends": "../../.eslintrc.js",

  "rules": {
    // The inspector is being migrated to HTML and cleaned of
    // chrome-privileged code, so this rule disallows requiring chrome
    // code. Some files here disable this rule still. The
    // goal is to enable the rule globally on all files.
    /* eslint-disable max-len */
    "mozilla/reject-some-requires": ["error", "^(chrome|chrome:.*|resource:.*|devtools/server/.*|.*\\.jsm)$"],
  },
};
