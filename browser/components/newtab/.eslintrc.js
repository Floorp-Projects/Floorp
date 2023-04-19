/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

module.exports = {
  // When adding items to this file please check for effects on sub-directories.
  parserOptions: {
    ecmaVersion: 2018,
    ecmaFeatures: {
      jsx: true,
    },
    sourceType: "module",
  },
  plugins: [
    "import", // require("eslint-plugin-import")
    "react", // require("eslint-plugin-react")
    "jsx-a11y", // require("eslint-plugin-jsx-a11y")
  ],
  settings: {
    react: {
      version: "16.2.0",
    },
  },
  extends: [
    "eslint:recommended",
    "plugin:jsx-a11y/recommended", // require("eslint-plugin-jsx-a11y")
    "plugin:mozilla/recommended", // require("eslint-plugin-mozilla") require("eslint-plugin-fetch-options") require("eslint-plugin-html") require("eslint-plugin-no-unsanitized")
    "prettier", // require("eslint-config-prettier")
  ],
  overrides: [
    {
      // These files use fluent-dom to insert content
      files: [
        "content-src/aboutwelcome/components/Zap.jsx",
        "content-src/aboutwelcome/components/MultiStageAboutWelcome.jsx",
        "content-src/aboutwelcome/components/MultiStageScreen.jsx",
        "content-src/aboutwelcome/components/MultiStageProtonScreen.jsx",
        "content-src/aboutwelcome/components/MultiSelect.jsx",
        "content-src/aboutwelcome/components/ReturnToAMO.jsx",
        "content-src/asrouter/templates/OnboardingMessage/**",
        "content-src/asrouter/templates/FirstRun/**",
        "content-src/components/TopSites/**",
        "content-src/components/MoreRecommendations/MoreRecommendations.jsx",
        "content-src/components/CollapsibleSection/CollapsibleSection.jsx",
        "content-src/components/DiscoveryStreamComponents/DSEmptyState/DSEmptyState.jsx",
        "content-src/components/DiscoveryStreamComponents/DSPrivacyModal/DSPrivacyModal.jsx",
        "content-src/components/CustomizeMenu/**",
      ],
      rules: {
        "jsx-a11y/anchor-has-content": "off",
        "jsx-a11y/heading-has-content": "off",
        "jsx-a11y/label-has-associated-control": "off",
        "jsx-a11y/no-onchange": "off",
      },
    },
    {
      files: [
        "bin/**",
        "content-src/**",
        "./*.js",
        "loaders/**",
        "tools/**",
        "test/unit/**",
      ],
      env: {
        node: true,
      },
    },
    {
      // Use a configuration that's more appropriate for JSMs
      files: "**/*.jsm",
      parserOptions: {
        sourceType: "script",
      },
      rules: {
        "no-implicit-globals": "off",
      },
    },
    {
      files: "test/xpcshell/**",
      extends: ["plugin:mozilla/xpcshell-test"],
    },
    {
      files: "test/browser/**",
      extends: ["plugin:mozilla/browser-test"],
    },
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
      // That are all files in browser/component/newtab/test that produces warnings in the existing test infrastructure.
      // Since our focus is that new tests won't use http without thinking twice we exempt
      // these test files for now.
      // TODO gradually check and remove from here bug 1758951.
      files: [
        "browser/components/newtab/test/browser/abouthomecache/browser_process_crash.js",
        "browser/components/newtab/test/browser/browser_aboutwelcome_observer.js",
        "browser/components/newtab/test/browser/browser_asrouter_cfr.js",
        "browser/components/newtab/test/browser/browser_asrouter_group_frequency.js",
        "browser/components/newtab/test/browser/browser_asrouter_group_userprefs.js",
        "browser/components/newtab/test/browser/browser_trigger_listeners.js",
      ],
      rules: {
        "@microsoft/sdl/no-insecure-url": "off",
      },
    },
  ],
  rules: {
    "fetch-options/no-fetch-credentials": "error",

    "react/jsx-boolean-value": ["error", "always"],
    "react/jsx-key": "error",
    "react/jsx-no-bind": "error",
    "react/jsx-no-comment-textnodes": "error",
    "react/jsx-no-duplicate-props": "error",
    "react/jsx-no-target-blank": "error",
    "react/jsx-no-undef": "error",
    "react/jsx-pascal-case": "error",
    "react/jsx-uses-react": "error",
    "react/jsx-uses-vars": "error",
    "react/no-access-state-in-setstate": "error",
    "react/no-danger": "error",
    "react/no-deprecated": "error",
    "react/no-did-mount-set-state": "error",
    "react/no-did-update-set-state": "error",
    "react/no-direct-mutation-state": "error",
    "react/no-is-mounted": "error",
    "react/no-unknown-property": "error",
    "react/require-render-return": "error",

    "accessor-pairs": ["error", { setWithoutGet: true, getWithoutSet: false }],
    "array-callback-return": "error",
    "block-scoped-var": "error",
    "consistent-this": ["error", "use-bind"],
    eqeqeq: "error",
    "for-direction": "error",
    "func-name-matching": "error",
    "getter-return": "error",
    "guard-for-in": "error",
    "handle-callback-err": "error",
    "lines-between-class-members": "error",
    "max-depth": ["error", 4],
    "max-nested-callbacks": ["error", 4],
    "max-params": ["error", 6],
    "max-statements": ["error", 50],
    "max-statements-per-line": ["error", { max: 2 }],
    "new-cap": ["error", { newIsCap: true, capIsNew: false }],
    "no-alert": "error",
    "no-buffer-constructor": "error",
    "no-console": ["error", { allow: ["error"] }],
    "no-div-regex": "error",
    "no-duplicate-imports": "error",
    "no-eq-null": "error",
    "no-extend-native": "error",
    "no-extra-label": "error",
    "no-implicit-coercion": ["error", { allow: ["!!"] }],
    "no-implicit-globals": "error",
    "no-loop-func": "error",
    "no-mixed-requires": "error",
    "no-multi-assign": "error",
    "no-multi-str": "error",
    "no-new": "error",
    "no-new-func": "error",
    "no-new-require": "error",
    "no-octal-escape": "error",
    "no-param-reassign": "error",
    "no-path-concat": "error",
    "no-process-exit": "error",
    "no-proto": "error",
    "no-prototype-builtins": "error",
    "no-return-assign": ["error", "except-parens"],
    "no-script-url": "error",
    "no-shadow": "error",
    "no-template-curly-in-string": "error",
    "no-undef-init": "error",
    "no-unmodified-loop-condition": "error",
    "no-unused-expressions": "error",
    "no-use-before-define": "error",
    "no-useless-computed-key": "error",
    "no-useless-constructor": "error",
    "no-useless-rename": "error",
    "no-var": "error",
    "no-void": ["error", { allowAsStatement: true }],
    "one-var": ["error", "never"],
    "operator-assignment": ["error", "always"],
    "prefer-destructuring": [
      "error",
      {
        AssignmentExpression: { array: true },
        VariableDeclarator: { array: true, object: true },
      },
    ],
    "prefer-numeric-literals": "error",
    "prefer-promise-reject-errors": "error",
    "prefer-rest-params": "error",
    "prefer-spread": "error",
    "prefer-template": "error",
    radix: ["error", "always"],
    "require-await": "error",
    "sort-vars": "error",
    "symbol-description": "error",
    "vars-on-top": "error",
    yoda: ["error", "never"],
  },
};
