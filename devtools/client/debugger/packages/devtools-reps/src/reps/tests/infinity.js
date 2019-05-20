/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const { shallow } = require("enzyme");

const { REPS, getRep } = require("../rep");

const { InfinityRep, Rep } = REPS;

const stubs = require("../stubs/infinity");

describe("testInfinity", () => {
  const stub = stubs.get("Infinity");

  it("Rep correctly selects Infinity Rep", () => {
    expect(getRep(stub)).toBe(InfinityRep.rep);
  });

  it("Infinity rep has expected text content for Infinity", () => {
    const renderedComponent = shallow(
      Rep({
        object: stub,
      })
    );
    expect(renderedComponent.text()).toEqual("Infinity");
  });
});

describe("testNegativeInfinity", () => {
  const stub = stubs.get("NegativeInfinity");

  it("Rep correctly selects Infinity Rep", () => {
    expect(getRep(stub)).toBe(InfinityRep.rep);
  });

  it("Infinity rep has expected text content for negative Infinity", () => {
    const renderedComponent = shallow(
      Rep({
        object: stub,
      })
    );
    expect(renderedComponent.text()).toEqual("-Infinity");
  });
});
