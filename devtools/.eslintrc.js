/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.exports = {
  plugins: ["react"],
  globals: {
    exports: true,
    isWorker: true,
    loader: true,
    module: true,
    reportError: true,
    require: true,
  },
  overrides: [
    {
      files: ["client/framework/**"],
      rules: {
        "no-return-assign": "off",
      },
    },
    {
      files: [
        // Allow non-camelcase so that run_test doesn't produce a warning.
        "**/test*/**/*",
      ],
      rules: {
        camelcase: "off",
      },
    },
    {
      files: ["client/framework/**"],
      rules: {
        "max-nested-callbacks": "off",
      },
    },
    {
      files: ["client/framework/**", "shared/webconsole/test/chrome/*.html"],
      rules: {
        "mozilla/no-aArgs": "off",
      },
    },
    {
      files: ["client/framework/test/**"],
      rules: {
        "mozilla/var-only-at-top-level": "off",
      },
    },
    {
      files: ["client/framework/**"],
      rules: {
        "no-shadow": "off",
      },
    },
    {
      files: ["client/framework/**"],
      rules: {
        strict: "off",
      },
    },
    {
      // For all head*.js files, turn off no-unused-vars at a global level
      files: ["**/head*.js"],
      rules: {
        "no-unused-vars": ["error", { args: "none", vars: "local" }],
      },
    },
    {
      // For all server and shared files, prevent requiring devtools/client
      // modules.
      files: ["server/**", "shared/**"],
      rules: {
        "mozilla/reject-some-requires": [
          "error",
          "^(resource://)?devtools/client",
        ],
      },
      excludedFiles: [
        // Tests can always import anything.
        "**/test*/**/*",
      ],
    },
    {
      // Cu, Cc etc... are not available in most devtools modules loaded by require.
      files: ["**"],
      excludedFiles: [
        // Enable the rule on JSM, test head files and some specific files.
        "**/*.jsm",
        "**/*.sjs",
        "**/test/**/head.js",
        "**/test/**/shared-head.js",
        "client/debugger/test/mochitest/code_frame-script.js",
        "client/responsive.html/browser/content.js",
        "server/actors/webconsole/content-process-forward.js",
        "server/startup/content-process.js",
        "server/startup/frame.js",
        "shared/loader/base-loader.js",
        "shared/loader/browser-loader.js",
        "shared/loader/worker-loader.js",
        "startup/aboutdebugging-registration.js",
        "startup/aboutdevtoolstoolbox-registration.js",
        "startup/devtools-startup.js",
      ],
      rules: {
        "mozilla/no-define-cc-etc": "off",
      },
    },
    {
      // All DevTools files should avoid relative paths.
      files: ["**"],
      excludedFiles: [
        // Debugger modules have a custom bundling logic which relies on relative
        // paths.
        "client/debugger/src/**",
        // `client/shared/build` contains node helpers to build the debugger and
        // not devtools modules.
        "client/shared/build/**",
      ],
      rules: {
        "mozilla/reject-relative-requires": "error",
      },
    },
    {
      // These tests use old React. We should accept deprecated API usages
      files: [
        "client/inspector/markup/test/doc_markup_events_react_development_15.4.1.html",
        "client/inspector/markup/test/doc_markup_events_react_development_15.4.1_jsx.html",
        "client/inspector/markup/test/doc_markup_events_react_production_15.3.1.html",
        "client/inspector/markup/test/doc_markup_events_react_production_15.3.1_jsx.html",
      ],
      rules: {
        "react/no-deprecated": "off",
      },
    },
  ],
  rules: {
    // These are the rules that have been configured so far to match the
    // devtools coding style.

    // Rules from the mozilla plugin
    "mozilla/balanced-observers": "error",
    "mozilla/no-aArgs": "error",
    // See bug 1224289.
    "mozilla/reject-importGlobalProperties": ["error", "everything"],
    "mozilla/var-only-at-top-level": "error",

    // Rules from the React plugin
    "react/display-name": "error",
    "react/no-danger": "error",
    "react/no-deprecated": "error",
    "react/no-did-mount-set-state": "error",
    "react/no-did-update-set-state": "error",
    "react/no-direct-mutation-state": "error",
    "react/no-unknown-property": "error",
    "react/prefer-es6-class": ["off", "always"],
    "react/prop-types": "error",
    "react/sort-comp": [
      "error",
      {
        order: ["static-methods", "lifecycle", "everything-else", "render"],
        groups: {
          lifecycle: [
            "displayName",
            "propTypes",
            "contextTypes",
            "childContextTypes",
            "mixins",
            "statics",
            "defaultProps",
            "constructor",
            "getDefaultProps",
            "getInitialState",
            "state",
            "getChildContext",
            "UNSAFE_componentWillMount",
            "componentDidMount",
            "UNSAFE_componentWillReceiveProps",
            "shouldComponentUpdate",
            "UNSAFE_componentWillUpdate",
            "componentDidUpdate",
            "componentWillUnmount",
          ],
        },
      },
    ],

    // Disallow using variables outside the blocks they are defined (especially
    // since only let and const are used, see "no-var").
    "block-scoped-var": "error",
    // Require camel case names
    camelcase: ["error", { properties: "never" }],
    // Warn about cyclomatic complexity in functions.
    // 20 is ESLint's default, and we want to keep it this way to prevent new highly
    // complex functions from being introduced. However, because Mozilla's eslintrc has
    // some other value defined, we need to override it here. See bug 1553449 for more
    // information on complex DevTools functions that are currently excluded.
    complexity: ["error", 20],
    // componentDidUnmount is not a real lifecycle method, use componentWillUnmount.
    "id-denylist": ["error", "componentDidUnmount"],
    // Maximum depth callbacks can be nested.
    "max-nested-callbacks": ["error", 3],
    // Require a capital letter for constructors, only check if all new
    // operators are followed by a capital letter. Don't warn when capitalized
    // functions are used without the new operator.
    "new-cap": ["error", { capIsNew: false }],
    // Disallow empty statements. This will report an error for:
    // try { something(); } catch (e) {}
    // but will not report it for:
    // try { something(); } catch (e) { /* Silencing the error because ...*/ }
    // which is a valid use case.
    "no-empty": "error",
    // Disallow adding to native types
    "no-extend-native": "error",
    // Disallow fallthrough of case statements, except if there is a comment.
    "no-fallthrough": "error",
    // Disallow use of multiline strings (use template strings instead).
    "no-multi-str": "error",
    // Disallow usage of __proto__ property.
    "no-proto": "error",
    // Prevent using some properties
    "no-restricted-properties": [
      "error",
      {
        property: "setupInParent",
        message: "avoid child/parent communication with setupInParent",
      },
    ],
    // Disallow use of assignment in return statement. It is preferable for a
    // single line of code to have only one easily predictable effect.
    "no-return-assign": "error",
    // Warn about declaration of variables already declared in the outer scope.
    // This isn't an error because it sometimes is useful to use the same name
    // in a small helper function rather than having to come up with another
    // random name.
    // Still, making this a warning can help people avoid being confused.
    "no-shadow": "error",
    // Disallow global and local variables that aren't used, but allow unused
    // function arguments.
    "no-unused-vars": ["error", { args: "none", vars: "all" }],
    // Enforce using `let` only when variables are reassigned.
    "prefer-const": ["error", { destructuring: "all" }],
    // Require use of the second argument for parseInt().
    radix: "error",
    // Require "use strict" to be defined globally in the script.
    strict: ["error", "global"],
    // Disallow Yoda conditions (where literal value comes first).
    yoda: "error",

    // And these are the rules that haven't been discussed so far, and that are
    // disabled for now until we introduce them, one at a time.

    // disallow overwriting functions written as function declarations
    "no-func-assign": "off",
    // disallow unnecessary nested blocks
    "no-lone-blocks": "off",
    // disallow unnecessary concatenation of literals or template literals
    "no-useless-concat": "off",
    // This rule will match any function starting with `use` which aren't
    // necessarily in a React component. Also DevTools aren't using React hooks
    // so this sounds unecessary.
    "react-hooks/rules-of-hooks": "off",
  },
  settings: {
    react: {
      version: "16.8",
    },
  },
};
