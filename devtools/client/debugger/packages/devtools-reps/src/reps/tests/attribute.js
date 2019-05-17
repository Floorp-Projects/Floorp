/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const { shallow } = require("enzyme");

const { REPS, getRep } = require("../rep");

const { expectActorAttribute } = require("./test-helpers");

const { Attribute, Rep } = REPS;

const stubs = require("../stubs/attribute");

describe("Attribute", () => {
  const stub = stubs.get("Attribute");

  it("Rep correctly selects Attribute Rep", () => {
    expect(getRep(stub)).toBe(Attribute.rep);
  });

  it("Attribute rep has expected text content", () => {
    const renderedComponent = shallow(
      Rep({
        object: stub,
      })
    );
    expect(renderedComponent.text()).toEqual(
      'class="autocomplete-suggestions"'
    );
    expectActorAttribute(renderedComponent, stub.actor);
  });
});
