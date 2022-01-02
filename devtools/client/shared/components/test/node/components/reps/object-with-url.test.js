/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

const { shallow } = require("enzyme");
const {
  REPS,
  getRep,
} = require("devtools/client/shared/components/reps/reps/rep");
const { ObjectWithURL } = REPS;
const stubs = require("devtools/client/shared/components/test/node/stubs/reps/object-with-url");
const {
  expectActorAttribute,
} = require("devtools/client/shared/components/test/node/components/reps/test-helpers");

describe("ObjectWithURL", () => {
  const stub = stubs.get("ObjectWithUrl");

  it("selects the correct rep", () => {
    expect(getRep(stub)).toEqual(ObjectWithURL.rep);
  });

  it("renders with correct class name and content", () => {
    const renderedComponent = shallow(
      ObjectWithURL.rep({
        object: stub,
        shouldRenderTooltip: true,
      })
    );
    expect(renderedComponent.text()).toBe(
      "Location https://www.mozilla.org/en-US/"
    );
    expect(renderedComponent.prop("title")).toBe(
      "Location https://www.mozilla.org/en-US/"
    );
    expect(renderedComponent.hasClass("objectBox-Location")).toBe(true);

    const innerNode = renderedComponent.find(".objectPropValue");
    expect(innerNode.text()).toBe("https://www.mozilla.org/en-US/");

    expectActorAttribute(renderedComponent, stub.actor);
  });
});
