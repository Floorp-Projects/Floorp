"use strict";

module.exports = {
  "plugins": [
    "react"
  ],
  "globals": {
    "atob": true,
    "btoa": true,
    "Components": true,
    "CSSRule": true,
    "clearInterval": true,
    "clearTimeout": true,
    "console": true,
    "CSS": true,
    "DocumentFragment": true,
    "DOMParser": true,
    "dump": true,
    "Element": true,
    "exports": true,
    "isWorker": true,
    "indexedDB": true,
    "loader": true,
    "module": true,
    "Node": true,
    "reportError": true,
    "require": true,
    "setInterval": true,
    "setTimeout": true,
    "uneval": true,
    "URL": true,
    "WebSocket": true,
    "XMLHttpRequest": true
  },
  "rules": {
    // These are the rules that have been configured so far to match the
    // devtools coding style.

    // Rules from the mozilla plugin
    "mozilla/no-aArgs": "warn",
    "mozilla/no-cpows-in-tests": "error",
    "mozilla/no-single-arg-cu-import": "error",
    // See bug 1224289.
    "mozilla/reject-importGlobalProperties": "error",
    // devtools/shared/platform is special; see the README.md in that
    // directory for details.  We reject requires using explicit
    // subdirectories of this directory.
    "mozilla/reject-some-requires": ["error", "^devtools/shared/platform/(chome|content)/"],
    "mozilla/var-only-at-top-level": "warn",

    // Rules from the React plugin
    "react/display-name": "error",
    "react/no-danger": "error",
    "react/no-did-mount-set-state": "error",
    "react/no-did-update-set-state": "error",
    "react/no-direct-mutation-state": "error",
    "react/no-unknown-property": "error",
    "react/prefer-es6-class": ["warn", "never"],
    "react/prop-types": "error",
    "react/sort-comp": ["error", {
      order: [
        "lifecycle",
        "everything-else",
        "render"
      ],
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
          "componentWillMount",
          "componentDidMount",
          "componentWillReceiveProps",
          "shouldComponentUpdate",
          "componentWillUpdate",
          "componentDidUpdate",
          "componentWillUnmount"
        ]
      }
    }],

    // Disallow using variables outside the blocks they are defined (especially
    // since only let and const are used, see "no-var").
    "block-scoped-var": "error",
    // Enforce one true brace style (opening brace on the same line) and avoid
    // start and end braces on the same line.
    "brace-style": ["error", "1tbs", {"allowSingleLine": false}],
    // Require camel case names
    "camelcase": "error",
    // Allow trailing commas for easy list extension.  Having them does not
    // impair readability, but also not required either.
    "comma-dangle": "off",
    // Enforce spacing before and after comma
    "comma-spacing": ["error", {"before": false, "after": true}],
    // Enforce one true comma style.
    "comma-style": ["error", "last"],
    // Warn about cyclomatic complexity in functions.
    "complexity": ["error", 35],
    // Require return statements to either always or never specify values.
    "consistent-return": "error",
    // Don't warn for inconsistent naming when capturing this (not so important
    // with auto-binding fat arrow functions).
    "consistent-this": "off",
    // Enforce curly brace conventions for all control statements.
    "curly": "error",
    // Don't require a default case in switch statements. Avoid being forced to
    // add a bogus default when you know all possible cases are handled.
    "default-case": "off",
    // Enforce dots on the next line with property name.
    "dot-location": ["error", "property"],
    // Encourage the use of dot notation whenever possible.
    "dot-notation": "error",
    // Enforce newline at the end of file, with no multiple empty lines.
    "eol-last": "error",
    // Allow using == instead of ===, in the interest of landing something since
    // the devtools codebase is split on convention here.
    "eqeqeq": "off",
    // Don't require function expressions to have a name.
    // This makes the code more verbose and hard to read. Our engine already
    // does a fantastic job assigning a name to the function, which includes
    // the enclosing function name, and worst case you have a line number that
    // you can just look up.
    "func-names": "off",
    // Allow use of function declarations and expressions.
    "func-style": "off",
    // Deprecated, will be removed in 1.0.
    "generator-star": "off",
    // Enforce the spacing around the * in generator functions.
    "generator-star-spacing": ["error", "after"],
    // Deprecated, will be removed in 1.0.
    "global-strict": "off",
    // Only useful in a node environment.
    "handle-callback-err": "off",
    // Tab width.
    "indent": ["error", 2, {"SwitchCase": 1, "ArrayExpression": "first", "ObjectExpression": "first"}],
    // Enforces spacing between keys and values in object literal properties.
    "key-spacing": ["error", {"beforeColon": false, "afterColon": true}],
    // Enforces unix style line breaks.
    "linebreak-style": ["error", "unix"],
    // Don't enforce the maximum depth that blocks can be nested. The complexity
    // rule is a better rule to check this.
    "max-depth": "off",
    // Maximum length of a line.
    "max-len": ["error", 90, 2, {
      "ignoreUrls": true,
      "ignorePattern": "data:image\/|\\s*require\\s*\\(|^\\s*loader\\.lazy|-\\*-"
    }],
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
    "new-cap": ["error", {"capIsNew": false}],
    // Disallow the omission of parentheses when invoking a constructor with no
    // arguments.
    "new-parens": "error",
    // Disallow use of the Array constructor.
    "no-array-constructor": "error",
    // Allow use of bitwise operators.
    "no-bitwise": "off",
    // Disallow use of arguments.caller or arguments.callee.
    "no-caller": "error",
    // Disallow the catch clause parameter name being the same as a variable in
    // the outer scope, to avoid confusion.
    "no-catch-shadow": "error",
    // Deprecated, will be removed in 1.0.
    "no-comma-dangle": "off",
    // Disallow assignment in conditional expressions.
    "no-cond-assign": "error",
    // Allow using the console API.
    "no-console": "off",
    // Allow using constant expressions in conditions like while (true)
    "no-constant-condition": "off",
    // Allow use of the continue statement.
    "no-continue": "off",
    // Disallow control characters in regular expressions.
    "no-control-regex": "error",
    // Disallow use of debugger.
    "no-debugger": "error",
    // Disallow deletion of variables (deleting properties is fine).
    "no-delete-var": "error",
    // Allow division operators explicitly at beginning of regular expression.
    "no-div-regex": "off",
    // Disallow duplicate arguments in functions.
    "no-dupe-args": "error",
    // Disallow duplicate keys when creating object literals.
    "no-dupe-keys": "error",
    // Disallow a duplicate case label.
    "no-duplicate-case": "error",
    // Disallow else after a return in an if. The else around the second return
    // here is useless:
    // if (something) { return false; } else { return true; }
    "no-else-return": "error",
    // Disallow empty statements. This will report an error for:
    // try { something(); } catch (e) {}
    // but will not report it for:
    // try { something(); } catch (e) { /* Silencing the error because ...*/ }
    // which is a valid use case.
    "no-empty": "error",
    // Disallow the use of empty character classes in regular expressions.
    "no-empty-character-class": "error",
    // Disallow use of eval(). We have other APIs to evaluate code in content.
    "no-eval": "error",
    // Disallow assigning to the exception in a catch block.
    "no-ex-assign": "error",
    // Disallow adding to native types
    "no-extend-native": "error",
    // Disallow unnecessary function binding.
    "no-extra-bind": "error",
    // Disallow double-negation boolean casts in a boolean context.
    "no-extra-boolean-cast": "error",
    // Allow unnecessary parentheses, as they may make the code more readable.
    "no-extra-parens": "off",
    // Disallow unnecessary semicolons.
    "no-extra-semi": "error",
    // Deprecated, will be removed in 1.0.
    "no-extra-strict": "off",
    // Disallow fallthrough of case statements, except if there is a comment.
    "no-fallthrough": "error",
    // Allow the use of leading or trailing decimal points in numeric literals.
    "no-floating-decimal": "off",
    // disallow use of eval()-like methods
    "no-implied-eval": "error",
    // Allow comments inline after code.
    "no-inline-comments": "off",
    // Disallow if as the only statement in an else block.
    "no-lonely-if": "error",
    // Allow mixing regular variable and require declarations (not a node env).
    "no-mixed-requires": "off",
    // Disallow mixed spaces and tabs for indentation.
    "no-mixed-spaces-and-tabs": "error",
    // Disallow use of multiple spaces (sometimes used to align const values,
    // array or object items, etc.). It's hard to maintain and doesn't add that
    // much benefit.
    "no-multi-spaces": "error",
    // Disallow use of multiline strings (use template strings instead).
    "no-multi-str": "error",
    // Disallow multiple empty lines.
    "no-multiple-empty-lines": ["error", {"max": 1}],
    // Disallow reassignments of native objects.
    "no-native-reassign": "error",
    // Disallow nested ternary expressions, they make the code hard to read.
    "no-nested-ternary": "error",
    // Allow use of new operator with the require function.
    "no-new-require": "off",
    // Disallow use of octal literals.
    "no-octal": "error",
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
    // Disallow declaring the same variable more than once (we use let anyway).
    "no-redeclare": "error",
    // Disallow multiple spaces in a regular expression literal.
    "no-regex-spaces": "off",
    // Allow reserved words being used as object literal keys.
    "no-reserved-keys": "off",
    // Don't restrict usage of specified node modules (not a node environment).
    "no-restricted-modules": "off",
    // Disallow use of assignment in return statement. It is preferable for a
    // single line of code to have only one easily predictable effect.
    "no-return-assign": "error",
    // Allow use of javascript: urls.
    "no-script-url": "off",
    // Disallow assignments like foo = foo
    "no-self-assign": "error",
    // Disallow comparisons where both sides are exactly the same.
    "no-self-compare": "error",
    // Disallow use of comma operator.
    "no-sequences": "error",
    // Warn about declaration of variables already declared in the outer scope.
    // This isn't an error because it sometimes is useful to use the same name
    // in a small helper function rather than having to come up with another
    // random name.
    // Still, making this a warning can help people avoid being confused.
    "no-shadow": "error",
    // Disallow shadowing of names such as arguments.
    "no-shadow-restricted-names": "error",
    // Deprecated, will be removed in 1.0.
    "no-space-before-semi": "off",
    // Disallow space between function identifier and application.
    "no-spaced-func": "error",
    // Disallow sparse arrays, eg. let arr = [,,2].
    // Array destructuring is fine though:
    // for (let [, breakpointPromise] of aPromises)
    "no-sparse-arrays": "error",
    // Allow use of synchronous methods (not a node environment).
    "no-sync": "off",
    // Allow the use of ternary operators.
    "no-ternary": "off",
    // Disallow throwing literals (eg. throw "error" instead of
    // throw new Error("error")).
    "no-throw-literal": "error",
    // Disallow trailing whitespace at the end of lines.
    "no-trailing-spaces": "error",
    // Disallow use of undeclared variables unless mentioned in a /*global */
    // block. Note that globals from head.js are automatically imported in tests
    // by the import-headjs-globals rule form the mozilla eslint plugin.
    "no-undef": "error",
    // Allow dangling underscores in identifiers (for privates).
    "no-underscore-dangle": "off",
    // Allow use of undefined variable.
    "no-undefined": "off",
    // Disallow the use of Boolean literals in conditional expressions.
    "no-unneeded-ternary": "error",
    // Disallow unreachable statements after a return, throw, continue, or break
    // statement.
    "no-unreachable": "error",
    // Disallow global and local variables that aren't used, but allow unused
    // function arguments.
    "no-unused-vars": ["error", {"vars": "all", "args": "none"}],
    // Disallow flow control that escapes from "finally".
    "no-unsafe-finally": "error",
    // Allow using variables before they are defined.
    "no-use-before-define": "off",
    // Disallow useless Function.prototype.{call/apply}
    "no-useless-call": "error",
    // Disallow useless return;
    "no-useless-return": "error",
    // We use var-only-at-top-level instead of no-var as we allow top level
    // vars.
    "no-var": "off",
    // Allow using TODO/FIXME comments.
    "no-warning-comments": "off",
    // Disallow use of the with statement.
    "no-with": "error",
    // Don't require method and property shorthand syntax for object literals.
    // We use this in the code a lot, but not consistently, and this seems more
    // like something to check at code review time.
    "object-shorthand": "off",
    // Allow more than one variable declaration per function.
    "one-var": "off",
    // Disallow padding within blocks.
    "padded-blocks": ["error", "never"],
    // Don't require quotes around object literal property names.
    "quote-props": "off",
    // Double quotes should be used.  Other quote characters can be used around strings
    // with embedded double quotes to avoid escaping them.  Template literals are allowed
    // mainly for building multi-line messages where only some lines use substitution.
    "quotes": ["error", "double", {"avoidEscape": true, "allowTemplateLiterals": true}],
    // Require use of the second argument for parseInt().
    "radix": "error",
    // Always require use of semicolons wherever they are valid.
    "semi": ["error", "always"],
    // Enforce spacing after semicolons.
    "semi-spacing": ["error", {"before": false, "after": true}],
    // Don't require to sort variables within the same declaration block.
    // Anyway, one-var is disabled.
    "sort-vars": "off",
    // Deprecated, will be removed in 1.0.
    "space-after-function-name": "off",
    // Require a space around all keywords.
    "keyword-spacing": "error",
    // Require a space before the start brace of a block.
    "space-before-blocks": ["error", "always"],
    // Deprecated, will be removed in 1.0.
    "space-before-function-parentheses": "off",
    // Require space after keyword for anonymous functions, but disallow space
    // after name of named functions.
    "space-before-function-paren": ["error", {"anonymous": "always", "named": "never"}],
    // Disable the rule that checks if spaces inside {} and [] are there or not.
    // Our code is split on conventions, and it'd be nice to have "error" rules
    // instead, one for [] and one for {}. So, disabling until we write them.
    "space-in-brackets": "off",
    // Disallow spaces inside parentheses.
    "space-in-parens": ["error", "never"],
    // Require spaces around operators, except for a|"off".
    "space-infix-ops": ["error", {"int32Hint": true}],
    // Require spaces before/after unary operators (words on by default,
    // nonwords off by default).
    "space-unary-ops": ["error", { "words": true, "nonwords": false }],
    // Deprecated, will be removed in 1.0.
    "space-unary-word-ops": "off",
    // Require a space immediately following the // in a line comment.
    "spaced-comment": ["error", "always"],
    // Require "use strict" to be defined globally in the script.
    "strict": ["error", "global"],
    // Disallow comparisons with the value NaN.
    "use-isnan": "error",
    // Warn about invalid JSDoc comments.
    // Disabled for now because of https://github.com/eslint/eslint/issues/2270
    // The rule fails on some jsdoc comments like in:
    // devtools/client/webconsole/console-output.js
    "valid-jsdoc": "off",
    // Ensure that the results of typeof are compared against a valid string.
    "valid-typeof": "error",
    // Allow vars to be declared anywhere in the scope.
    "vars-on-top": "off",
    // Don't require immediate function invocation to be wrapped in parentheses.
    "wrap-iife": "off",
    // Don't require regex literals to be wrapped in parentheses (which
    // supposedly prevent them from being mistaken for division operators).
    "wrap-regex": "off",
    // Disallow Yoda conditions (where literal value comes first).
    "yoda": "error",

    // And these are the rules that haven't been discussed so far, and that are
    // disabled for now until we introduce them, one at a time.

    // Require for-in loops to have an if statement.
    "guard-for-in": "off",
    // allow/disallow an empty newline after var statement
    "newline-after-var": "off",
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
    // disallow usage of __iterator__ property
    "no-iterator": "off",
    // disallow labels that share a name with a variable
    "no-label-var": "off",
    // disallow use of labeled statements
    "no-labels": "error",
    // disallow unnecessary nested blocks
    "no-lone-blocks": "off",
    // disallow creation of functions within loops
    "no-loop-func": "off",
    // disallow negation of the left operand of an in expression
    "no-negated-in-lhs": "off",
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
    // disallow use of void operator
    "no-void": "off",
    // disallow wrapping of non-IIFE statements in parens
    "no-wrap-func": "off",
    // require assignment operator shorthand where possible or prohibit it
    // entirely
    "operator-assignment": "off",
    // enforce operators to be placed before or after line breaks
    "operator-linebreak": "off",
  }
};
