/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

const { shallow } = require("enzyme");
const {
  REPS,
  getRep,
} = require("resource://devtools/client/shared/components/reps/reps/rep.js");
const { Number, Rep } = REPS;
const stubs = require("resource://devtools/client/shared/components/test/node/stubs/reps/number.js");

describe("Int", () => {
  const stub = stubs.get("Int");

  it("correctly selects Number Rep for Integer value", () => {
    expect(getRep(stub)).toBe(Number.rep);
  });

  it("renders with expected text content for integer", () => {
    const renderedComponent = shallow(
      Rep({
        object: stub,
        shouldRenderTooltip: true,
      })
    );

    expect(renderedComponent.text()).toEqual("5");
    expect(renderedComponent.prop("title")).toBe("5");
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
        shouldRenderTooltip: true,
      })
    );

    expect(renderedComponent.text()).toEqual("true");
    expect(renderedComponent.prop("title")).toBe("true");
  });

  it("renders with expected text content for boolean false", () => {
    const renderedComponent = shallow(
      Rep({
        object: stubFalse,
        shouldRenderTooltip: true,
      })
    );

    expect(renderedComponent.text()).toEqual("false");
    expect(renderedComponent.prop("title")).toBe("false");
  });
});

describe("Negative Zero", () => {
  const stubNegativeZeroGrip = stubs.get("NegZeroGrip");
  const stubNegativeZeroValue = -0;

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
        shouldRenderTooltip: true,
      })
    );

    expect(renderedComponent.text()).toEqual("-0");
    expect(renderedComponent.prop("title")).toBe("-0");
  });

  it("renders with expected text content for negative zero value", () => {
    const renderedComponent = shallow(
      Rep({
        object: stubNegativeZeroValue,
        shouldRenderTooltip: true,
      })
    );

    expect(renderedComponent.text()).toEqual("-0");
    expect(renderedComponent.prop("title")).toBe("-0");
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
        shouldRenderTooltip: true,
      })
    );

    expect(renderedComponent.text()).toEqual("0");
    expect(renderedComponent.prop("title")).toBe("0");
  });
});

describe("Unsafe Int", () => {
  it("renders with expected test content for a long number", () => {
    const renderedComponent = shallow(
      Rep({
        // eslint-disable-next-line no-loss-of-precision
        object: 900719925474099122,
        shouldRenderTooltip: true,
      })
    );

    expect(renderedComponent.text()).toEqual("900719925474099100");
    expect(renderedComponent.prop("title")).toBe("900719925474099100");
  });
});
