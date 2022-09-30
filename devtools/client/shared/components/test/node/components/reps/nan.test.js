/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

const { shallow } = require("enzyme");

const {
  REPS,
  getRep,
} = require("resource://devtools/client/shared/components/reps/reps/rep.js");

const { NaNRep, Rep } = REPS;

const stubs = require("resource://devtools/client/shared/components/test/node/stubs/reps/nan.js");

describe("NaN", () => {
  const stub = stubs.get("NaN");

  it("selects NaN Rep as expected", () => {
    expect(getRep(stub)).toBe(NaNRep.rep);
  });

  it("renders NaN Rep as expected", () => {
    const renderedComponent = shallow(
      Rep({
        object: stub,
      })
    );
    expect(renderedComponent).toMatchSnapshot();
  });

  it("NaN rep renders with the correct title element", () => {
    const renderedComponent = shallow(
      Rep({
        object: stub,
        shouldRenderTooltip: true,
      })
    );
    expect(renderedComponent.prop("title")).toBe("NaN");
  });
});
