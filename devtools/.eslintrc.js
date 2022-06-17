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
        "client/shared/*.jsm",
        // Allow non-camelcase so that run_test doesn't produce a warning.
        "**/test*/**/*",
      ],
      rules: {
        camelcase: "off",
      },
    },
    {
      files: [
        "client/framework/**",
        "client/shared/*.jsm",
        "client/shared/widgets/*.jsm",
        "client/storage/VariablesView.jsm",
      ],
      rules: {
        "consistent-return": "off",
      },
    },
    {
      files: ["client/framework/**"],
      rules: {
        "max-nested-callbacks": "off",
      },
    },
    {
      files: [
        "client/framework/**",
        "client/shared/*.jsm",
        "client/shared/widgets/*.jsm",
        "client/storage/VariablesView.jsm",
        "shared/webconsole/test/chrome/*.html",
      ],
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
      files: [
        "client/framework/**",
        "client/shared/widgets/*.jsm",
        "client/storage/VariablesView.jsm",
      ],
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
    // Don't warn for inconsistent naming when capturing this (not so important
    // with auto-binding fat arrow functions).
    "consistent-this": "off",
    // Don't require a default case in switch statements. Avoid being forced to
    // add a bogus default when you know all possible cases are handled.
    "default-case": "off",
    // Allow using == instead of ===, in the interest of landing something since
    // the devtools codebase is split on convention here.
    eqeqeq: "off",
    // Don't require function expressions to have a name.
    // This makes the code more verbose and hard to read. Our engine already
    // does a fantastic job assigning a name to the function, which includes
    // the enclosing function name, and worst case you have a line number that
    // you can just look up.
    "func-names": "off",
    // Allow use of function declarations and expressions.
    "func-style": "off",
    // Only useful in a node environment.
    "handle-callback-err": "off",
    // componentDidUnmount is not a real lifecycle method, use componentWillUnmount.
    "id-denylist": ["error", "componentDidUnmount"],
    // Maximum depth callbacks can be nested.
    "max-nested-callbacks": ["error", 3],
    // Don't limit the number of parameters that can be used in a function.
    "max-params": "off",
    // Don't limit the maximum number of statement allowed in a function. We
    // already have the complexity rule that's a better measurement.
    "max-statements": "off",
    // Require a capital letter for constructors, only check if all new
    // operators are followed by a capital letter. Don't warn when capitalized
    // functions are used without the new operator.
    "new-cap": ["error", { capIsNew: false }],
    // Allow use of bitwise operators.
    "no-bitwise": "off",
    // Allow using the console API.
    "no-console": "off",
    // Allow using constant expressions in conditions like while (true)
    "no-constant-condition": "off",
    // Allow use of the continue statement.
    "no-continue": "off",
    // Allow division operators explicitly at beginning of regular expression.
    "no-div-regex": "off",
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
    // Allow comments inline after code.
    "no-inline-comments": "off",
    // Allow mixing regular variable and require declarations (not a node env).
    "no-mixed-requires": "off",
    // Disallow use of multiline strings (use template strings instead).
    "no-multi-str": "error",
    // Allow use of new operator with the require function.
    "no-new-require": "off",
    // Allow reassignment of function parameters.
    "no-param-reassign": "off",
    // Allow string concatenation with __dirname and __filename (not a node env).
    "no-path-concat": "off",
    // Allow use of unary operators, ++ and --.
    "no-plusplus": "off",
    // Allow using process.env (not a node environment).
    "no-process-env": "off",
    // Allow using process.exit (not a node environment).
    "no-process-exit": "off",
    // Disallow usage of __proto__ property.
    "no-proto": "error",
    // Disallow multiple spaces in a regular expression literal.
    "no-regex-spaces": "off",
    // Don't restrict usage of specified node modules (not a node environment).
    "no-restricted-modules": "off",
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
    // Allow use of javascript: urls.
    "no-script-url": "off",
    // Warn about declaration of variables already declared in the outer scope.
    // This isn't an error because it sometimes is useful to use the same name
    // in a small helper function rather than having to come up with another
    // random name.
    // Still, making this a warning can help people avoid being confused.
    "no-shadow": "error",
    // Allow use of synchronous methods (not a node environment).
    "no-sync": "off",
    // Allow the use of ternary operators.
    "no-ternary": "off",
    // Allow dangling underscores in identifiers (for privates).
    "no-underscore-dangle": "off",
    // Allow use of undefined variable.
    "no-undefined": "off",
    // Disallow global and local variables that aren't used, but allow unused
    // function arguments.
    "no-unused-vars": ["error", { args: "none", vars: "all" }],
    // Allow using variables before they are defined.
    "no-use-before-define": "off",
    // We use var-only-at-top-level instead of no-var as we allow top level
    // vars.
    "no-var": "off",
    // Allow using TODO/FIXME comments.
    "no-warning-comments": "off",
    // Don't require method and property shorthand syntax for object literals.
    // We use this in the code a lot, but not consistently, and this seems more
    // like something to check at code review time.
    "object-shorthand": "off",
    // Allow more than one variable declaration per function.
    "one-var": "off",
    // Enforce using `let` only when variables are reassigned.
    "prefer-const": ["error", { destructuring: "all" }],
    // Require use of the second argument for parseInt().
    radix: "error",
    // Don't require to sort variables within the same declaration block.
    // Anyway, one-var is disabled.
    "sort-vars": "off",
    // Require "use strict" to be defined globally in the script.
    strict: ["error", "global"],
    // Warn about invalid JSDoc comments.
    // Disabled for now because of https://github.com/eslint/eslint/issues/2270
    // The rule fails on some jsdoc comments like in:
    // devtools/client/webconsole/old/console-output.js
    "valid-jsdoc": "off",
    // Allow vars to be declared anywhere in the scope.
    "vars-on-top": "off",
    // Disallow Yoda conditions (where literal value comes first).
    yoda: "error",

    // And these are the rules that haven't been discussed so far, and that are
    // disabled for now until we introduce them, one at a time.

    // Require for-in loops to have an if statement.
    "guard-for-in": "off",
    // disallow the use of alert, confirm, and prompt
    "no-alert": "off",
    // disallow comparisons to null without a type-checking operator
    "no-eq-null": "off",
    // disallow overwriting functions written as function declarations
    "no-func-assign": "off",
    // disallow function or variable declarations in nested blocks
    "no-inner-declarations": "off",
    // disallow invalid regular expression strings in the RegExp constructor
    "no-invalid-regexp": "off",
    // disallow irregular whitespace outside of strings and comments
    "no-irregular-whitespace": "off",
    // disallow labels that share a name with a variable
    "no-label-var": "off",
    // disallow unnecessary nested blocks
    "no-lone-blocks": "off",
    // disallow creation of functions within loops
    "no-loop-func": "off",
    // disallow use of new operator when not part of the assignment or
    // comparison
    "no-new": "off",
    // disallow use of new operator for Function object
    "no-new-func": "off",
    // disallow use of the Object constructor
    "no-new-object": "off",
    // disallows creating new instances of String,Number, and Boolean
    "no-new-wrappers": "off",
    // disallow the use of object properties of the global object (Math and
    // JSON) as functions
    "no-obj-calls": "off",
    // disallow use of octal escape sequences in string literals, such as
    // var foo = "Copyright \251";
    "no-octal-escape": "off",
    // disallow use of undefined when initializing variables
    "no-undef-init": "off",
    // disallow usage of expressions in statement position
    "no-unused-expressions": "off",
    // disallow unnecessary concatenation of literals or template literals
    "no-useless-concat": "off",
    // disallow use of void operator
    "no-void": "off",
    // require assignment operator shorthand where possible or prohibit it
    // entirely
    "operator-assignment": "off",
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
