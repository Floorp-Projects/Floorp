/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

const { shallow } = require("enzyme");

const {
  REPS,
  getRep,
} = require("resource://devtools/client/shared/components/reps/reps/rep.js");

const { Undefined, Rep } = REPS;

const stubs = require("resource://devtools/client/shared/components/test/node/stubs/reps/undefined.js");
// Test that correct rep is chosen
describe("Test Undefined", () => {
  const stub = stubs.get("Undefined");

  it("selects Undefined as expected", () => {
    expect(getRep(stub)).toBe(Undefined.rep);
  });

  it("Rep correctly selects Undefined Rep for plain JS undefined", () => {
    expect(getRep(undefined, undefined, true)).toBe(Undefined.rep);
  });

  it("Undefined rep has expected text content", () => {
    const renderedComponent = shallow(
      Rep({
        object: stub,
      })
    );
    expect(renderedComponent.text()).toEqual("undefined");
  });

  it("Undefined rep has expected class names", () => {
    const renderedComponent = shallow(
      Rep({
        object: stub,
      })
    );
    expect(renderedComponent.hasClass("objectBox objectBox-undefined")).toEqual(
      true
    );
  });

  it("Undefined rep has expected title", () => {
    const renderedComponent = shallow(
      Rep({
        object: stub,
        shouldRenderTooltip: true,
      })
    );
    expect(renderedComponent.prop("title")).toEqual("undefined");
  });
});
