/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { wrapExpression, sanitizeInput, getValue } from "../expressions";

function createError(preview) {
  return {
    value: { result: { class: "Error", preview } }
  };
}

describe("expressions", () => {
  describe("wrap exxpression", () => {
    it("should wrap an expression", () => {
      expect(wrapExpression("foo")).toMatchSnapshot();
    });

    it("should wrap expression with a comment", () => {
      expect(wrapExpression("foo // yo yo")).toMatchSnapshot();
    });

    it("should wrap quotes", () => {
      expect(wrapExpression('"2"')).toMatchSnapshot();
    });
  });

  describe("sanitize input", () => {
    it("sanitizes quotes", () => {
      expect(sanitizeInput('foo"')).toEqual('foo"');
    });

    it("sanitizes 2 quotes", () => {
      expect(sanitizeInput('"3"')).toEqual('"3"');
    });

    it("evaluates \\u{61} as a", () => {
      expect(sanitizeInput("\u{61}")).toEqual("a");
    });

    it("evaluates N\\u{61}N as NaN", () => {
      expect(sanitizeInput("N\u{61}N")).toEqual("NaN");
    });
  });

  describe("getValue", () => {
    it("Reference Errors should be shown as (unavailable)", () => {
      expect(
        getValue(createError({ name: "ReferenceError" })).value.unavailable
      ).toEqual(true);
    });

    it("Errors messages should be shown", () => {
      expect(
        getValue(createError({ name: "Foo", message: "YO" })).value
      ).toEqual("Foo: YO");
    });
  });
});
