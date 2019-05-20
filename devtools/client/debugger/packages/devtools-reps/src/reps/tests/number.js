/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const { shallow } = require("enzyme");
const { REPS, getRep } = require("../rep");
const { Number, Rep } = REPS;
const stubs = require("../stubs/number");

describe("Int", () => {
  const stub = stubs.get("Int");

  it("correctly selects Number Rep for Integer value", () => {
    expect(getRep(stub)).toBe(Number.rep);
  });

  it("renders with expected text content for integer", () => {
    const renderedComponent = shallow(
      Rep({
        object: stub,
      })
    );

    expect(renderedComponent.text()).toEqual("5");
  });
});

describe("Boolean", () => {
  const stubTrue = stubs.get("True");
  const stubFalse = stubs.get("False");

  it("correctly selects Number Rep for boolean value", () => {
    expect(getRep(stubTrue)).toBe(Number.rep);
  });

  it("renders with expected text content for boolean true", () => {
    const renderedComponent = shallow(
      Rep({
        object: stubTrue,
      })
    );

    expect(renderedComponent.text()).toEqual("true");
  });

  it("renders with expected text content for boolean false", () => {
    const renderedComponent = shallow(
      Rep({
        object: stubFalse,
      })
    );

    expect(renderedComponent.text()).toEqual("false");
  });
});

describe("Negative Zero", () => {
  const stubNegativeZeroGrip = stubs.get("NegZeroGrip");
  const stubNegativeZeroValue = stubs.get("NegZeroValue");

  it("correctly selects Number Rep for negative zero grip", () => {
    expect(getRep(stubNegativeZeroGrip)).toBe(Number.rep);
  });

  it("correctly selects Number Rep for negative zero value", () => {
    expect(getRep(stubNegativeZeroValue)).toBe(Number.rep);
  });

  it("renders with expected text content for negative zero grip", () => {
    const renderedComponent = shallow(
      Rep({
        object: stubNegativeZeroGrip,
      })
    );

    expect(renderedComponent.text()).toEqual("-0");
  });

  it("renders with expected text content for negative zero value", () => {
    const renderedComponent = shallow(
      Rep({
        object: stubNegativeZeroValue,
      })
    );

    expect(renderedComponent.text()).toEqual("-0");
  });
});

describe("Zero", () => {
  it("correctly selects Number Rep for zero value", () => {
    expect(getRep(0)).toBe(Number.rep);
  });

  it("renders with expected text content for zero value", () => {
    const renderedComponent = shallow(
      Rep({
        object: 0,
      })
    );

    expect(renderedComponent.text()).toEqual("0");
  });
});

describe("Unsafe Int", () => {
  const stub = stubs.get("UnsafeInt");

  it("renders with expected test content for a long number", () => {
    const renderedComponent = shallow(
      Rep({
        object: stub,
      })
    );

    expect(renderedComponent.text()).toEqual("900719925474099100");
  });
});
