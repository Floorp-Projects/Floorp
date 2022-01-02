/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

const { shallow } = require("enzyme");

const {
  REPS,
  getRep,
} = require("devtools/client/shared/components/reps/reps/rep");

const {
  expectActorAttribute,
} = require("devtools/client/shared/components/test/node/components/reps/test-helpers");

const { Attribute, Rep } = REPS;

const stubs = require("devtools/client/shared/components/test/node/stubs/reps/attribute");

describe("Attribute", () => {
  const stub = stubs.get("Attribute");

  it("Rep correctly selects Attribute Rep", () => {
    expect(getRep(stub)).toBe(Attribute.rep);
  });

  it("Attribute rep has expected text content", () => {
    const renderedComponent = shallow(
      Rep({
        object: stub,
        shouldRenderTooltip: true,
      })
    );
    expect(renderedComponent.text()).toEqual(
      'class="autocomplete-suggestions"'
    );
    expect(renderedComponent.prop("title")).toBe(
      'class="autocomplete-suggestions"'
    );
    expectActorAttribute(renderedComponent, stub.actor);
  });
});
