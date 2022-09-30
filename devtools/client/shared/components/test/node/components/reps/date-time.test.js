/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

const { shallow } = require("enzyme");

const {
  REPS,
  getRep,
} = require("resource://devtools/client/shared/components/reps/reps/rep.js");

const {
  expectActorAttribute,
} = require("resource://devtools/client/shared/components/test/node/components/reps/test-helpers.js");

const { DateTime, Rep } = REPS;

const stubs = require("resource://devtools/client/shared/components/test/node/stubs/reps/date-time.js");

describe("test DateTime", () => {
  const stub = stubs.get("DateTime")._grip;

  it("selects DateTime as expected", () => {
    expect(getRep(stub)).toBe(DateTime.rep);
  });

  it("renders DateTime as expected", () => {
    const renderedComponent = shallow(
      Rep({
        object: stub,
        shouldRenderTooltip: true,
      })
    );

    const expectedDate = new Date(
      "Date Thu Mar 31 2016 00:17:24 GMT+0300 (EAT)"
    ).toString();

    expect(renderedComponent.text()).toEqual(`Date ${expectedDate}`);
    expect(renderedComponent.prop("title")).toEqual(`Date ${expectedDate}`);
    expectActorAttribute(renderedComponent, stub.actor);
  });
});

describe("test invalid DateTime", () => {
  const stub = stubs.get("InvalidDateTime")._grip;

  it("renders expected text for invalid date", () => {
    const renderedComponent = shallow(
      Rep({
        object: stub,
        shouldRenderTooltip: true,
      })
    );

    expect(renderedComponent.text()).toEqual("Invalid Date");
    expect(renderedComponent.prop("title")).toEqual("Invalid Date");
  });
});
