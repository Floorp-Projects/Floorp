/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import mapExpression from "../mapExpression";
import { format } from "prettier";
import cases from "jest-in-case";

function test({
  expression,
  newExpression,
  bindings,
  mappings,
  shouldMapExpression,
  expectedMapped
}) {
  const res = mapExpression(
    expression,
    mappings,
    bindings,
    shouldMapExpression
  );
  expect(
    format(res.expression, {
      parser: "babylon"
    })
  ).toEqual(format(newExpression, { parser: "babylon" }));
  expect(res.mapped).toEqual(expectedMapped);
}

function formatAwait(body) {
  return `(async () => { ${body} })();`;
}

describe("mapExpression", () => {
  cases("mapExpressions", test, [
    {
      name: "await",
      expression: "await a()",
      newExpression: formatAwait("return await a()"),
      bindings: [],
      mappings: {},
      shouldMapExpression: true,
      expectedMapped: {
        await: true,
        bindings: false,
        originalExpression: false
      }
    },
    {
      name: "await (multiple statements)",
      expression: "const x = await a(); x + x",
      newExpression: formatAwait("self.x = await a(); return x + x;"),
      bindings: [],
      mappings: {},
      shouldMapExpression: true,
      expectedMapped: {
        await: true,
        bindings: true,
        originalExpression: false
      }
    },
    {
      name: "await (inner)",
      expression: "async () => await a();",
      newExpression: "async () => await a();",
      bindings: [],
      mappings: {},
      shouldMapExpression: true,
      expectedMapped: {
        await: false,
        bindings: false,
        originalExpression: false
      }
    },
    {
      name: "await (multiple awaits)",
      expression: "const x = await a(); await b(x)",
      newExpression: formatAwait("self.x = await a(); return await b(x);"),
      bindings: [],
      mappings: {},
      shouldMapExpression: true,
      expectedMapped: {
        await: true,
        bindings: true,
        originalExpression: false
      }
    },
    {
      name: "await (assignment)",
      expression: "let x = await sleep(100, 2)",
      newExpression: formatAwait("return (self.x = await sleep(100, 2))"),
      bindings: [],
      mappings: {},
      shouldMapExpression: true,
      expectedMapped: {
        await: true,
        bindings: true,
        originalExpression: false
      }
    },
    {
      name: "await (destructuring)",
      expression: "const { a, c: y } = await b()",
      newExpression: formatAwait(
        "return ({ a: self.a, c: self.y } = await b())"
      ),
      bindings: [],
      mappings: {},
      shouldMapExpression: true,
      expectedMapped: {
        await: true,
        bindings: true,
        originalExpression: false
      }
    },
    {
      name: "await (array destructuring)",
      expression: "const [a, y] = await b();",
      newExpression: formatAwait("return ([self.a, self.y] = await b())"),
      bindings: [],
      mappings: {},
      shouldMapExpression: true,
      expectedMapped: {
        await: true,
        bindings: true,
        originalExpression: false
      }
    },
    {
      name: "await (mixed destructuring)",
      expression: "const [{ a }] = await b();",
      newExpression: formatAwait("return ([{ a: self.a }] = await b())"),
      bindings: [],
      mappings: {},
      shouldMapExpression: true,
      expectedMapped: {
        await: true,
        bindings: true,
        originalExpression: false
      }
    },
    {
      name: "await (destructuring, multiple statements)",
      expression: "const { a, c: y } = await b(), { x } = await y()",
      newExpression: formatAwait(`
        ({ a: self.a, c: self.y } = await b())
        return ({ x: self.x } = await y());
      `),
      bindings: [],
      mappings: {},
      shouldMapExpression: true,
      expectedMapped: {
        await: true,
        bindings: true,
        originalExpression: false
      }
    },
    {
      name: "await (destructuring, bindings)",
      expression: "const { a, c: y } = await b();",
      newExpression: formatAwait("return ({ a, c: y } = await b())"),
      bindings: ["a", "y"],
      mappings: {},
      shouldMapExpression: true,
      expectedMapped: {
        await: true,
        bindings: true,
        originalExpression: false
      }
    },
    {
      name: "await (array destructuring, bindings)",
      expression: "const [a, y] = await b();",
      newExpression: formatAwait("return ([a, y] = await b())"),
      bindings: ["a", "y"],
      mappings: {},
      shouldMapExpression: true,
      expectedMapped: {
        await: true,
        bindings: true,
        originalExpression: false
      }
    },
    {
      name: "await (mixed destructuring, bindings)",
      expression: "const [{ a }] = await b();",
      newExpression: formatAwait("return ([{ a }] = await b())"),
      bindings: ["a"],
      mappings: {},
      shouldMapExpression: true,
      expectedMapped: {
        await: true,
        bindings: true,
        originalExpression: false
      }
    },
    {
      name: "await (destructuring with defaults, bindings)",
      expression: "const { c, a = 5 } = await b();",
      newExpression: formatAwait("return ({ c: self.c, a = 5 } = await b())"),
      bindings: ["a", "y"],
      mappings: {},
      shouldMapExpression: true,
      expectedMapped: {
        await: true,
        bindings: true,
        originalExpression: false
      }
    },
    {
      name: "await (array destructuring with defaults, bindings)",
      expression: "const [a, y = 10] = await b();",
      newExpression: formatAwait("return ([a, y = 10] = await b())"),
      bindings: ["a", "y"],
      mappings: {},
      shouldMapExpression: true,
      expectedMapped: {
        await: true,
        bindings: true,
        originalExpression: false
      }
    },
    {
      name: "await (mixed destructuring with defaults, bindings)",
      expression: "const [{ c = 5 }, a = 5] = await b();",
      newExpression: formatAwait(
        "return ([ { c: self.c = 5 }, a = 5] = await b())"
      ),
      bindings: ["a"],
      mappings: {},
      shouldMapExpression: true,
      expectedMapped: {
        await: true,
        bindings: true,
        originalExpression: false
      }
    },
    {
      name: "await (nested destructuring, bindings)",
      expression: "const { a, c: { y } } = await b();",
      newExpression: formatAwait(`
       return ({
          a,
          c: { y }
        } = await b());
    `),
      bindings: ["a", "y"],
      mappings: {},
      shouldMapExpression: true,
      expectedMapped: {
        await: true,
        bindings: true,
        originalExpression: false
      }
    },
    {
      name: "await (nested destructuring with defaults)",
      expression: "const { a, c: { y = 5 } = {} } = await b();",
      newExpression: formatAwait(`return ({
        a: self.a,
        c: { y: self.y = 5 } = {},
      } = await b());
    `),
      bindings: [],
      mappings: {},
      shouldMapExpression: true,
      expectedMapped: {
        await: true,
        bindings: true,
        originalExpression: false
      }
    },
    {
      name: "await (very nested destructuring with defaults)",
      expression:
        "const { a, c: { y: { z = 10, b } = { b: 5 } } } = await b();",
      newExpression: formatAwait(`
        return ({
          a: self.a,
          c: {
            y: { z: self.z = 10, b: self.b } = {
              b: 5
            }
          }
        } = await b());
    `),
      bindings: [],
      mappings: {},
      shouldMapExpression: true,
      expectedMapped: {
        await: true,
        bindings: true,
        originalExpression: false
      }
    },
    {
      name: "simple",
      expression: "a",
      newExpression: "a",
      bindings: [],
      mappings: {},
      shouldMapExpression: true,
      expectedMapped: {
        await: false,
        bindings: false,
        originalExpression: false
      }
    },
    {
      name: "mappings",
      expression: "a",
      newExpression: "_a",
      bindings: [],
      mappings: {
        a: "_a"
      },
      shouldMapExpression: true,
      expectedMapped: {
        await: false,
        bindings: false,
        originalExpression: true
      }
    },
    {
      name: "declaration",
      expression: "var a = 3;",
      newExpression: "self.a = 3",
      bindings: [],
      mappings: {},
      shouldMapExpression: true,
      expectedMapped: {
        await: false,
        bindings: true,
        originalExpression: false
      }
    },
    {
      name: "declaration + destructuring",
      expression: "var { a } = { a: 3 };",
      newExpression: "({ a: self.a } = {\n a: 3 \n})",
      bindings: [],
      mappings: {},
      shouldMapExpression: true,
      expectedMapped: {
        await: false,
        bindings: true,
        originalExpression: false
      }
    },
    {
      name: "bindings",
      expression: "var a = 3;",
      newExpression: "a = 3",
      bindings: ["a"],
      mappings: {},
      shouldMapExpression: true,
      expectedMapped: {
        await: false,
        bindings: true,
        originalExpression: false
      }
    },
    {
      name: "bindings + destructuring",
      expression: "var { a } = { a: 3 };",
      newExpression: "({ a } = { \n a: 3 \n })",
      bindings: ["a"],
      mappings: {},
      shouldMapExpression: true,
      expectedMapped: {
        await: false,
        bindings: true,
        originalExpression: false
      }
    },
    {
      name: "bindings + destructuring + rest",
      expression: "var { a, ...foo } = {}",
      newExpression: "({ a, ...self.foo } = {})",
      bindings: ["a"],
      mappings: {},
      shouldMapExpression: true,
      expectedMapped: {
        await: false,
        bindings: true,
        originalExpression: false
      }
    },
    {
      name: "bindings + array destructuring + rest",
      expression: "var [a, ...foo] = []",
      newExpression: "([a, ...self.foo] = [])",
      bindings: ["a"],
      mappings: {},
      shouldMapExpression: true,
      expectedMapped: {
        await: false,
        bindings: true,
        originalExpression: false
      }
    },
    {
      name: "bindings + mappings",
      expression: "a = 3;",
      newExpression: "self.a = 3",
      bindings: ["_a"],
      mappings: { a: "_a" },
      shouldMapExpression: true,
      expectedMapped: {
        await: false,
        bindings: true,
        originalExpression: false
      }
    },
    {
      name: "bindings + mappings + destructuring",
      expression: "var { a } = { a: 4 }",
      newExpression: "({ a: self.a } = {\n a: 4 \n})",
      bindings: ["_a"],
      mappings: { a: "_a" },
      shouldMapExpression: true,
      expectedMapped: {
        await: false,
        bindings: true,
        originalExpression: false
      }
    },
    {
      name: "bindings without mappings",
      expression: "a = 3;",
      newExpression: "a = 3",
      bindings: [],
      mappings: { a: "_a" },
      shouldMapExpression: false,
      expectedMapped: {
        await: false,
        bindings: false,
        originalExpression: false
      }
    },
    {
      name: "object destructuring + bindings without mappings",
      expression: "({ a } = {});",
      newExpression: "({ a: _a } = {})",
      bindings: [],
      mappings: { a: "_a" },
      shouldMapExpression: false,
      expectedMapped: {
        await: false,
        bindings: false,
        originalExpression: true
      }
    }
  ]);
});
