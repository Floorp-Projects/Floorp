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
  env: {
    node: true,
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
    "plugin:mozilla/browser-test",
    "plugin:mozilla/mochitest-test",
    "plugin:prettier/recommended", // require("eslint-plugin-prettier")
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
        "jsx-a11y/anchor-has-content": 0,
        "jsx-a11y/heading-has-content": 0,
        "jsx-a11y/label-has-associated-control": 0,
        "jsx-a11y/no-onchange": 0,
      },
    },
    {
      // Use a configuration that's more appropriate for JSMs
      files: "**/*.jsm",
      parserOptions: {
        sourceType: "script",
      },
      env: {
        node: false,
      },
      rules: {
        "no-implicit-globals": 0,
      },
    },
    {
      files: "test/xpcshell/**",
      extends: ["plugin:mozilla/xpcshell-test"],
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
    "fetch-options/no-fetch-credentials": 2,

    "react/jsx-boolean-value": [2, "always"],
    "react/jsx-key": 2,
    "react/jsx-no-bind": 2,
    "react/jsx-no-comment-textnodes": 2,
    "react/jsx-no-duplicate-props": 2,
    "react/jsx-no-target-blank": 2,
    "react/jsx-no-undef": 2,
    "react/jsx-pascal-case": 2,
    "react/jsx-uses-react": 2,
    "react/jsx-uses-vars": 2,
    "react/no-access-state-in-setstate": 2,
    "react/no-danger": 2,
    "react/no-deprecated": 2,
    "react/no-did-mount-set-state": 2,
    "react/no-did-update-set-state": 2,
    "react/no-direct-mutation-state": 2,
    "react/no-is-mounted": 2,
    "react/no-unknown-property": 2,
    "react/require-render-return": 2,

    "accessor-pairs": [2, { setWithoutGet: true, getWithoutSet: false }],
    "array-callback-return": 2,
    "block-scoped-var": 2,
    "callback-return": 0,
    camelcase: 0,
    "capitalized-comments": 0,
    "class-methods-use-this": 0,
    "consistent-this": [2, "use-bind"],
    "default-case": 0,
    eqeqeq: 2,
    "for-direction": 2,
    "func-name-matching": 2,
    "func-names": 0,
    "func-style": 0,
    "getter-return": 2,
    "global-require": 0,
    "guard-for-in": 2,
    "handle-callback-err": 2,
    "id-blacklist": 0,
    "id-length": 0,
    "id-match": 0,
    "init-declarations": 0,
    "line-comment-position": 0,
    "lines-between-class-members": 2,
    "max-depth": [2, 4],
    "max-lines": 0,
    "max-nested-callbacks": [2, 4],
    "max-params": [2, 6],
    "max-statements": [2, 50],
    "max-statements-per-line": [2, { max: 2 }],
    "multiline-comment-style": 0,
    "new-cap": [2, { newIsCap: true, capIsNew: false }],
    "newline-after-var": 0,
    "newline-before-return": 0,
    "no-alert": 2,
    "no-await-in-loop": 0,
    "no-bitwise": 0,
    "no-buffer-constructor": 2,
    "no-catch-shadow": 2,
    "no-console": 1,
    "no-continue": 0,
    "no-div-regex": 2,
    "no-duplicate-imports": 2,
    "no-empty-function": 0,
    "no-eq-null": 2,
    "no-extend-native": 2,
    "no-extra-label": 2,
    "no-implicit-coercion": [2, { allow: ["!!"] }],
    "no-implicit-globals": 2,
    "no-inline-comments": 0,
    "no-invalid-this": 0,
    "no-label-var": 2,
    "no-loop-func": 2,
    "no-magic-numbers": 0,
    "no-mixed-requires": 2,
    "no-multi-assign": 2,
    "no-multi-str": 2,
    "no-negated-condition": 0,
    "no-negated-in-lhs": 2,
    "no-new": 2,
    "no-new-func": 2,
    "no-new-require": 2,
    "no-octal-escape": 2,
    "no-param-reassign": 2,
    "no-path-concat": 2,
    "no-plusplus": 0,
    "no-process-env": 0,
    "no-process-exit": 2,
    "no-proto": 2,
    "no-prototype-builtins": 2,
    "no-restricted-globals": 0,
    "no-restricted-imports": 0,
    "no-restricted-modules": 0,
    "no-restricted-properties": 0,
    "no-restricted-syntax": 0,
    "no-return-assign": [2, "except-parens"],
    "no-script-url": 2,
    "no-shadow": 2,
    "no-sync": 0,
    "no-template-curly-in-string": 2,
    "no-ternary": 0,
    "no-undef-init": 2,
    "no-undefined": 0,
    "no-underscore-dangle": 0,
    "no-unmodified-loop-condition": 2,
    "no-unused-expressions": 2,
    "no-use-before-define": 2,
    "no-useless-computed-key": 2,
    "no-useless-constructor": 2,
    "no-useless-rename": 2,
    "no-var": 2,
    "no-void": [2, { allowAsStatement: true }],
    "no-warning-comments": 0, // TODO: Change to `1`?
    "one-var": [2, "never"],
    "operator-assignment": [2, "always"],
    "padding-line-between-statements": 0,
    "prefer-const": 0, // TODO: Change to `1`?
    "prefer-destructuring": [
      2,
      {
        AssignmentExpression: { array: true },
        VariableDeclarator: { array: true, object: true },
      },
    ],
    "prefer-numeric-literals": 2,
    "prefer-promise-reject-errors": 2,
    "prefer-reflect": 0,
    "prefer-rest-params": 2,
    "prefer-spread": 2,
    "prefer-template": 2,
    radix: [2, "always"],
    "require-await": 2,
    "require-jsdoc": 0,
    "sort-keys": 0,
    "sort-vars": 2,
    strict: 0,
    "symbol-description": 2,
    "valid-jsdoc": [
      0,
      {
        requireReturn: false,
        requireParamDescription: false,
        requireReturnDescription: false,
      },
    ],
    "vars-on-top": 2,
    yoda: [2, "never"],
  },
};
