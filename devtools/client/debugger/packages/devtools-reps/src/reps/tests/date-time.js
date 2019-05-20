/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const { shallow } = require("enzyme");

const { REPS, getRep } = require("../rep");

const { expectActorAttribute } = require("./test-helpers");

const { DateTime, Rep } = REPS;

const stubs = require("../stubs/date-time");

describe("test DateTime", () => {
  const stub = stubs.get("DateTime");

  it("selects DateTime as expected", () => {
    expect(getRep(stub)).toBe(DateTime.rep);
  });

  it("renders DateTime as expected", () => {
    const renderedComponent = shallow(
      Rep({
        object: stub,
      })
    );

    const expectedDate = new Date(
      "Date Thu Mar 31 2016 00:17:24 GMT+0300 (EAT)"
    ).toString();
    expect(renderedComponent.text()).toEqual(`Date ${expectedDate}`);
    expectActorAttribute(renderedComponent, stub.actor);
  });
});

describe("test invalid DateTime", () => {
  const stub = stubs.get("InvalidDateTime");

  it("renders expected text for invalid date", () => {
    const renderedComponent = shallow(
      Rep({
        object: stub,
      })
    );

    expect(renderedComponent.text()).toEqual("Invalid Date");
  });
});
