"use strict";

module.exports = {
  "rules": {
    "block-scoped-var": "error",
    "comma-dangle": ["error", "always-multiline"],
    "complexity": ["error", {"max": 21}],
    "indent-legacy": ["error", 2, {"SwitchCase": 1, "ArrayExpression": "first", "ObjectExpression": "first"}],
    "max-nested-callbacks": ["error", 3],
    "new-parens": "error",
    "no-extend-native": "error",
    "no-fallthrough": ["error", { "commentPattern": ".*[Ii]ntentional(?:ly)?\\s+fall(?:ing)?[\\s-]*through.*" }],
    "no-multi-str": "error",
    "no-return-assign": "error",
    "no-sequences": "error",
    "no-shadow": "error",
    "no-throw-literal": "error",
    "no-unused-vars": ["error", { "varsIgnorePattern": "^C[ciur]$" }],
    "padded-blocks": ["error", "never"],
    "semi-spacing": ["error", {"before": false, "after": true}],
    "space-in-parens": ["error", "never"],
    "strict": ["error", "global"],
    "yoda": "error"
  }
};
