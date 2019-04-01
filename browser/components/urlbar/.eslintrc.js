"use strict";

module.exports = {
  rules: {
    "mozilla/var-only-at-top-level": "error",
    "require-jsdoc": ["error", {
        "require": {
            "FunctionDeclaration": false,
            "MethodDefinition": false,
            "ClassDeclaration": true,
            "ArrowFunctionExpression": false,
            "FunctionExpression": false
        }
    }],
    "valid-jsdoc": ["error", {
      prefer: {
        return: "returns",
      },
      preferType: {
        Boolean: "boolean",
        Number: "number",
        String: "string",
        Object: "object",
        bool: "boolean",
      },
      requireParamDescription: false,
      requireReturn: false,
      requireReturnDescription: false,
    }],
    "no-unused-expressions": "error",
  }
};
