/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.exports = {
  plugins: ["spidermonkey-js"],

  overrides: [
    {
      files: ["*.js"],
      excludedFiles: ".eslintrc.js",
      processor: "spidermonkey-js/processor",
      env: {
        // Disable all built-in environments.
        node: false,
        browser: false,
        builtin: false,

        // We need to explicitly disable the default environments added from
        // "tools/lint/eslint/eslint-plugin-mozilla/lib/configs/recommended.js".
        es2021: false,
        "mozilla/privileged": false,

        // Enable SpiderMonkey's self-hosted environment.
        "spidermonkey-js/environment": true,
      },

      parserOptions: {
        ecmaVersion: "latest",
        sourceType: "script",

        // Self-hosted code defaults to strict mode.
        ecmaFeatures: {
          impliedStrict: true,
        },

        // Strict mode has to be enabled separately for the Babel parser.
        babelOptions: {
          parserOpts: {
            strictMode: true,
          },
        },
      },

      globals: {
        // The bytecode compiler special-cases these identifiers.
        allowContentIter: "readonly",
        callContentFunction: "readonly",
        callFunction: "readonly",
        constructContentFunction: "readonly",
        DefineDataProperty: "readonly",
        forceInterpreter: "readonly",
        GetBuiltinConstructor: "readonly",
        GetBuiltinPrototype: "readonly",
        GetBuiltinSymbol: "readonly",
        getPropertySuper: "readonly",
        hasOwn: "readonly",
        resumeGenerator: "readonly",
        SetCanonicalName: "readonly",
        SetIsInlinableLargeFunction: "readonly",
        ToNumeric: "readonly",
        ToString: "readonly",

        // We've disabled all built-in environments, which also removed
        // `undefined` from the list of globals. Put it back because it's
        // actually allowed in self-hosted code.
        undefined: "readonly",

        // Disable globals from stage 2/3 proposals for which we have work in
        // progress patches. Eventually these will be part of a future ES
        // release, in which case we can remove these extra entries.
        AsyncIterator: "off",
        Iterator: "off",
        Record: "off",
        Temporal: "off",
        Tuple: "off",

        // Undefine globals from Mozilla recommended file
        // "tools/lint/eslint/eslint-plugin-mozilla/lib/configs/recommended.js".
        Cc: "off",
        ChromeUtils: "off",
        Ci: "off",
        Components: "off",
        Cr: "off",
        Cu: "off",
        Debugger: "off",
        InstallTrigger: "off",
        InternalError: "off",
        Services: "off",
        dump: "off",
        openDialog: "off",
        uneval: "off",
      },
    },
  ],

  rules: {
    // We should fix those at some point, but we use this to detect NaNs.
    "no-self-compare": "off",
    "no-lonely-if": "off",
    // Disabled until we can use let/const to fix those erorrs,
    // and undefined names cause an exception and abort during runtime initialization.
    "no-redeclare": "off",
  },
};
