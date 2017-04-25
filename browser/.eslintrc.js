"use strict";

module.exports = {
  "extends": [
    "plugin:mozilla/recommended"
  ],

  "rules": {
    // XXX Bug 1326071 - This should be reduced down - probably to 20 or to
    // be removed & synced with the mozilla/recommended value.
    "complexity": ["error", {"max": 40}],

    // Disallow empty statements. This will report an error for:
    // try { something(); } catch (e) {}
    // but will not report it for:
    // try { something(); } catch (e) { /* Silencing the error because ...*/ }
    // which is a valid use case.
    "no-empty": "error",

    // Maximum depth callbacks can be nested.
    "max-nested-callbacks": ["error", 8],

    // Disallow adding to native types
    "no-extend-native": "error",

    "no-mixed-spaces-and-tabs": "error",
    "no-shadow": "error",
  }
};
