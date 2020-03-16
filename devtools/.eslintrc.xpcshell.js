// Parent config file for all devtools xpcshell files.
module.exports = {
  "extends": [
    "plugin:mozilla/xpcshell-test"
  ],
  "rules": {
    // Allow non-camelcase so that run_test doesn't produce a warning.
    "camelcase": "off",
    "block-scoped-var": "off",
    // Tests don't have to cleanup observers
    "mozilla/balanced-observers": 0,
    // Tests can always import anything.
    "mozilla/reject-some-requires": "off",
  }
};
