/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.exports = {
  plugins: ["spidermonkey-js"],

  overrides: [
    {
      files: ["*.js"],
      processor: "spidermonkey-js/processor",
      env: {
        "spidermonkey-js/environment": true,
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
      },
    },
  ],

  rules: {
    // We should fix those at some point, but we use this to detect NaNs.
    "no-self-compare": "off",
    "no-lonely-if": "off",
    // Manually defining all the selfhosted methods is a slog.
    "no-undef": "off",
    // Disabled until we can use let/const to fix those erorrs,
    // and undefined names cause an exception and abort during runtime initialization.
    "no-redeclare": "off",
  },
};
