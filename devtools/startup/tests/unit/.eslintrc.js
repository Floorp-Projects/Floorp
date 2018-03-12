// This file was copied from the .eslintrc.xpcshell.js
// This new xpcshell test folder should stay in mozilla-central while devtools move to a
// GitHub repository, hence the duplication.
module.exports = {
  "extends": [
    "plugin:mozilla/xpcshell-test"
  ],
  "rules": {
    // Allow non-camelcase so that run_test doesn't produce a warning.
    "camelcase": "off",
    // Allow using undefined variables so that tests can refer to functions
    // and variables defined in head.js files, without having to maintain a
    // list of globals in each .eslintrc file.
    // Note that bug 1168340 will eventually help auto-registering globals
    // from head.js files.
    "no-undef": "off",
    "block-scoped-var": "off",
    // Tests can always import anything.
    "mozilla/reject-some-requires": "off",
  }
};
