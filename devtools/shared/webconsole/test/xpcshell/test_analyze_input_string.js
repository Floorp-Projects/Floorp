// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/

"use strict";
const {
  analyzeInputString,
} = require("resource://devtools/shared/webconsole/analyze-input-string.js");

add_task(() => {
  const tests = [
    {
      desc: "simple property access",
      input: `var a = {b: 1};a.b`,
      expected: {
        isElementAccess: false,
        isPropertyAccess: true,
        expressionBeforePropertyAccess: `var a = {b: 1};a`,
        lastStatement: "a.b",
        mainExpression: `a`,
        matchProp: `b`,
      },
    },
    {
      desc: "deep property access",
      input: `a.b.c`,
      expected: {
        isElementAccess: false,
        isPropertyAccess: true,
        expressionBeforePropertyAccess: `a.b`,
        lastStatement: "a.b.c",
        mainExpression: `a.b`,
        matchProp: `c`,
      },
    },
    {
      desc: "element access",
      input: `a["b`,
      expected: {
        isElementAccess: true,
        isPropertyAccess: true,
        expressionBeforePropertyAccess: `a`,
        lastStatement: `a["b`,
        mainExpression: `a`,
        matchProp: `"b`,
      },
    },
    {
      desc: "element access without quotes",
      input: `a[b`,
      expected: {
        isElementAccess: true,
        isPropertyAccess: true,
        expressionBeforePropertyAccess: `a`,
        lastStatement: `a[b`,
        mainExpression: `a`,
        matchProp: `b`,
      },
    },
    {
      desc: "simple optional chaining access",
      input: `a?.b`,
      expected: {
        isElementAccess: false,
        isPropertyAccess: true,
        expressionBeforePropertyAccess: `a`,
        lastStatement: `a?.b`,
        mainExpression: `a`,
        matchProp: `b`,
      },
    },
    {
      desc: "deep optional chaining access",
      input: `a?.b?.c`,
      expected: {
        isElementAccess: false,
        isPropertyAccess: true,
        expressionBeforePropertyAccess: `a?.b`,
        lastStatement: `a?.b?.c`,
        mainExpression: `a?.b`,
        matchProp: `c`,
      },
    },
    {
      desc: "optional chaining element access",
      input: `a?.["b`,
      expected: {
        isElementAccess: true,
        isPropertyAccess: true,
        expressionBeforePropertyAccess: `a`,
        lastStatement: `a?.["b`,
        mainExpression: `a`,
        matchProp: `"b`,
      },
    },
    {
      desc: "optional chaining element access without quotes",
      input: `a?.[b`,
      expected: {
        isElementAccess: true,
        isPropertyAccess: true,
        expressionBeforePropertyAccess: `a`,
        lastStatement: `a?.[b`,
        mainExpression: `a`,
        matchProp: `b`,
      },
    },
    {
      desc: "deep optional chaining element access with quotes",
      input: `var a = {b: 1, c: ["."]};     a?.["b"]?.c?.["d[.`,
      expected: {
        isElementAccess: true,
        isPropertyAccess: true,
        expressionBeforePropertyAccess: `var a = {b: 1, c: ["."]};     a?.["b"]?.c`,
        lastStatement: `a?.["b"]?.c?.["d[.`,
        mainExpression: `a?.["b"]?.c`,
        matchProp: `"d[.`,
      },
    },
    {
      desc: "literal arrays with newline",
      input: `[1,2,3,\n4\n].`,
      expected: {
        isElementAccess: false,
        isPropertyAccess: true,
        expressionBeforePropertyAccess: `[1,2,3,\n4\n]`,
        lastStatement: `[1,2,3,4].`,
        mainExpression: `[1,2,3,4]`,
        matchProp: ``,
      },
    },
    {
      desc: "number literal with newline",
      input: `1\n.`,
      expected: {
        isElementAccess: false,
        isPropertyAccess: true,
        expressionBeforePropertyAccess: `1\n`,
        lastStatement: `1\n.`,
        mainExpression: `1`,
        matchProp: ``,
      },
    },
    {
      desc: "string literal",
      input: `"abc".`,
      expected: {
        isElementAccess: false,
        isPropertyAccess: true,
        expressionBeforePropertyAccess: `"abc"`,
        lastStatement: `"abc".`,
        mainExpression: `"abc"`,
        matchProp: ``,
      },
    },
    {
      desc: "string literal containing backslash",
      input: `"\\n".`,
      expected: {
        isElementAccess: false,
        isPropertyAccess: true,
        expressionBeforePropertyAccess: `"\\n"`,
        lastStatement: `"\\n".`,
        mainExpression: `"\\n"`,
        matchProp: ``,
      },
    },
    {
      desc: "single quote string literal containing backslash",
      input: `'\\r'.`,
      expected: {
        isElementAccess: false,
        isPropertyAccess: true,
        expressionBeforePropertyAccess: `'\\r'`,
        lastStatement: `'\\r'.`,
        mainExpression: `'\\r'`,
        matchProp: ``,
      },
    },
    {
      desc: "template string literal containing backslash",
      input: "`\\\\`.",
      expected: {
        isElementAccess: false,
        isPropertyAccess: true,
        expressionBeforePropertyAccess: "`\\\\`",
        lastStatement: "`\\\\`.",
        mainExpression: "`\\\\`",
        matchProp: ``,
      },
    },
    {
      desc: "unterminated double quote string literal",
      input: `"\n`,
      expected: {
        err: "unterminated string literal",
      },
    },
    {
      desc: "unterminated single quote string literal",
      input: `'\n`,
      expected: {
        err: "unterminated string literal",
      },
    },
    {
      desc: "optional chaining operator with spaces",
      input: `test  ?.    ["propA"]  ?.   [0]  ?.   ["propB"]  ?.  ['to`,
      expected: {
        isElementAccess: true,
        isPropertyAccess: true,
        expressionBeforePropertyAccess: `test  ?.    ["propA"]  ?.   [0]  ?.   ["propB"]  `,
        lastStatement: `test  ?.    ["propA"]  ?.   [0]  ?.   ["propB"]  ?.  ['to`,
        mainExpression: `test  ?.    ["propA"]  ?.   [0]  ?.   ["propB"]`,
        matchProp: `'to`,
      },
    },
  ];

  for (const { input, desc, expected } of tests) {
    const result = analyzeInputString(input);
    for (const [key, value] of Object.entries(expected)) {
      Assert.equal(value, result[key], `${desc} | ${key} has expected value`);
    }
  }
});
