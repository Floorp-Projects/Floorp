/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const { shallow } = require("enzyme");

const { REPS, getRep } = require("../rep");

const { Null, Rep } = REPS;

const stubs = require("../stubs/null");

describe("testNull", () => {
  const stub = stubs.get("Null");

  it("Rep correctly selects Null Rep", () => {
    expect(getRep(stub)).toBe(Null.rep);
  });

  it("Rep correctly selects Null Rep for plain JS null object", () => {
    expect(getRep(null, undefined, true)).toBe(Null.rep);
  });

  it("Null rep has expected text content", () => {
    const renderedComponent = shallow(
      Rep({
        object: stub,
      })
    );
    expect(renderedComponent.text()).toEqual("null");
  });
});
