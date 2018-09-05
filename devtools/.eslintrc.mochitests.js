// Parent config file for all devtools browser mochitest files.
module.exports = {
  "extends": [
    "plugin:mozilla/browser-test"
  ],
  // All globals made available in the test environment.
  "globals": {
    "DevToolsUtils": true,
    "gDevTools": true,
    "once": true,
    "synthesizeKeyFromKeyTag": true,
    "TargetFactory": true,
    "waitForTick": true,
    "waitUntilState": true,
  },

  "parserOptions": {
    "ecmaFeatures": {
      "jsx": true,
    }
  },

  "rules": {
    // Allow non-camelcase so that run_test doesn't produce a warning.
    "camelcase": "off",
    // Tests can always import anything.
    "mozilla/reject-some-requires": 0,
  },
};
