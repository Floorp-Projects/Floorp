/* eslint-disable import/no-commonjs */
// This config doesn't inhert from top-level eslint config  Bug 1780031

const xpcshellTestPaths = ["./unit*/**", "./xpcshell/**"];
module.exports = {
  env: {
    mocha: true,
  },
  globals: {
    assert: true,
    chai: true,
    sinon: true,
  },
  rules: {
    "func-name-matching": 0,
    "import/no-commonjs": 2,
    "lines-between-class-members": 0,
    "react/jsx-no-bind": 0,
    "require-await": 0,
  },
  overrides: [
    {
      // Exempt all files without a 'test' string in their path name since no-insecure-url
      // is focussing on the test base
      files: "*",
      excludedFiles: ["**/test**", "**/test*/**", "Test*/**"],
      rules: {
        "@microsoft/sdl/no-insecure-url": "off",
      },
    },
    {
      // Disable "no-insecure-url" for all xpcshell test
      files: xpcshellTestPaths.map(path => `${path}`),
      rules: {
        // As long "new HttpServer()" does not support https there is no reason to log warnings
        // https://bugzilla.mozilla.org/show_bug.cgi?id=1742061
        "@microsoft/sdl/no-insecure-url": "off",
      },
    },
  ],
};
