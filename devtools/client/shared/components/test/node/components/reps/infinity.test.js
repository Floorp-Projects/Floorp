/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

const { shallow } = require("enzyme");

const {
  REPS,
  getRep,
} = require("resource://devtools/client/shared/components/reps/reps/rep.js");

const { InfinityRep, Rep } = REPS;

const stubs = require("resource://devtools/client/shared/components/test/node/stubs/reps/infinity.js");

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

  it("Infinity rep has expected title content for Infinity", () => {
    const renderedComponent = shallow(
      Rep({
        object: stub,
        shouldRenderTooltip: true,
      })
    );
    expect(renderedComponent.prop("title")).toEqual("Infinity");
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

  it("Infinity rep has expected title content for negative Infinity", () => {
    const renderedComponent = shallow(
      Rep({
        object: stub,
        shouldRenderTooltip: true,
      })
    );
    expect(renderedComponent.prop("title")).toEqual("-Infinity");
  });
});
