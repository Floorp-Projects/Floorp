/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const { shallow } = require("enzyme");
const { REPS, getRep } = require("../rep");
const { BigInt, Rep } = REPS;
const stubs = require("../stubs/big-int");

describe("BigInt", () => {
  describe("1n", () => {
    const stub = stubs.get("1n");

    it("correctly selects BigInt Rep for BigInt value", () => {
      expect(getRep(stub)).toBe(BigInt.rep);
    });

    it("renders with expected text content for BigInt", () => {
      const renderedComponent = shallow(
        Rep({
          object: stub,
        })
      );

      expect(renderedComponent.text()).toEqual("1n");
    });
  });

  describe("-2n", () => {
    const stub = stubs.get("-2n");

    it("correctly selects BigInt Rep for negative BigInt value", () => {
      expect(getRep(stub)).toBe(BigInt.rep);
    });

    it("renders with expected text content for negative BigInt", () => {
      const renderedComponent = shallow(
        Rep({
          object: stub,
        })
      );

      expect(renderedComponent.text()).toEqual("-2n");
    });
  });

  describe("0n", () => {
    const stub = stubs.get("0n");

    it("correctly selects BigInt Rep for zero BigInt value", () => {
      expect(getRep(stub)).toBe(BigInt.rep);
    });

    it("renders with expected text content for zero BigInt", () => {
      const renderedComponent = shallow(Rep({ object: stub }));
      expect(renderedComponent.text()).toEqual("0n");
    });
  });

  describe("in objects", () => {
    it("renders with expected text content in Array", () => {
      const stub = stubs.get("[1n,-2n,0n]");
      const renderedComponent = shallow(Rep({ object: stub }));
      expect(renderedComponent.text()).toEqual("Array(3) [ 1n, -2n, 0n ]");
    });

    it("renders with expected text content in Set", () => {
      const stub = stubs.get("new Set([1n,-2n,0n])");
      const renderedComponent = shallow(Rep({ object: stub }));
      expect(renderedComponent.text()).toEqual("Set(3) [ 1n, -2n, 0n ]");
    });

    it("renders with expected text content in Map", () => {
      const stub = stubs.get("new Map([ [1n, -1n], [-2n, 0n], [0n, -2n]])");
      const renderedComponent = shallow(Rep({ object: stub }));
      expect(renderedComponent.text()).toEqual(
        "Map(3) { 1n → -1n, -2n → 0n, 0n → -2n }"
      );
    });

    it("renders with expected text content in Object", () => {
      const stub = stubs.get("({simple: 1n, negative: -2n, zero: 0n})");
      const renderedComponent = shallow(Rep({ object: stub }));
      expect(renderedComponent.text()).toEqual(
        "Object { simple: 1n, negative: -2n, zero: 0n }"
      );
    });

    it("renders with expected text content in Promise", () => {
      const stub = stubs.get("Promise.resolve(1n)");
      const renderedComponent = shallow(Rep({ object: stub }));
      expect(renderedComponent.text()).toEqual(
        'Promise { <state>: "fulfilled", <value>: 1n }'
      );
    });
  });
});
