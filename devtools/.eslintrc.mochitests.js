// Parent config file for all devtools browser mochitest files.
module.exports = {
  "extends": [
    "../testing/mochitest/browser.eslintrc.js"
  ],
  // All globals made available in the test environment.
  "globals": {
    "DevToolsUtils": true,
    "gDevTools": true,
    "getChromeDir": false,
    "getResolvedURI": false,
    "once": true,
    "synthesizeKeyFromKeyTag": true,
    "TargetFactory": true,
    "waitForTick": true,
  },

  "parserOptions": {
    "ecmaFeatures": {
      "jsx": true,
    }
  },

  "rules": {
    // Tests can always import anything.
    "mozilla/reject-some-requires": 0,
  },
};
