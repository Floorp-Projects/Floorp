/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import mapExpression from "../mapExpression";
import { format } from "prettier";

const formatOutput = output =>
  format(output, {
    parser: "babel",
  });

const mapOriginalExpression = (expression, mappings) =>
  mapExpression(expression, mappings, [], false, false).expression;

describe("mapOriginalExpression", () => {
  it("simple", () => {
    const generatedExpression = mapOriginalExpression("a + b;", {
      a: "foo",
      b: "bar",
    });
    expect(generatedExpression).toEqual("foo + bar;");
  });

  it("this", () => {
    const generatedExpression = mapOriginalExpression("this.prop;", {
      this: "_this",
    });
    expect(generatedExpression).toEqual("_this.prop;");
  });

  it("member expressions", () => {
    const generatedExpression = mapOriginalExpression("a + b", {
      a: "_mod.foo",
      b: "_mod.bar",
    });
    expect(generatedExpression).toEqual("_mod.foo + _mod.bar;");
  });

  it("block", () => {
    // todo: maybe wrap with parens ()
    const generatedExpression = mapOriginalExpression("{a}", {
      a: "_mod.foo",
      b: "_mod.bar",
    });
    expect(generatedExpression).toEqual("{\n  _mod.foo;\n}");
  });

  it("skips codegen with no mappings", () => {
    const generatedExpression = mapOriginalExpression("a + b", {
      a: "a",
      c: "_c",
    });
    expect(generatedExpression).toEqual("a + b");
  });

  it("object destructuring", () => {
    const generatedExpression = mapOriginalExpression("({ a } = { a: 4 })", {
      a: "_mod.foo",
    });

    expect(formatOutput(generatedExpression)).toEqual(
      formatOutput("({ a: _mod.foo } = {\n a: 4 \n})")
    );
  });

  it("nested object destructuring", () => {
    const generatedExpression = mapOriginalExpression(
      "({ a: { b, c } } = { a: 4 })",
      {
        a: "_mod.foo",
        b: "_mod.bar",
      }
    );

    expect(formatOutput(generatedExpression)).toEqual(
      formatOutput("({ a: { b: _mod.bar, c } } = {\n a: 4 \n})")
    );
  });

  it("shadowed bindings", () => {
    const generatedExpression = mapOriginalExpression(
      "window.thing = function fn(){ var a; a; b; }; a; b; ",
      {
        a: "_a",
        b: "_b",
      }
    );
    expect(generatedExpression).toEqual(
      "window.thing = function fn() {\n  var a;\n  a;\n  _b;\n};\n\n_a;\n_b;"
    );
  });
});
