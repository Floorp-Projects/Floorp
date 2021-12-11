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

const { Document } = REPS;
const stubs = require("devtools/client/shared/components/test/node/stubs/reps/document");

describe("Document", () => {
  const stub = stubs.get("Document");
  it("correctly selects Document Rep", () => {
    expect(getRep(stub)).toBe(Document.rep);
  });

  it("renders with expected text content", () => {
    const renderedComponent = shallow(
      Document.rep({
        object: stub,
        shouldRenderTooltip: true,
      })
    );

    expect(renderedComponent.text()).toEqual(
      "HTMLDocument https://www.mozilla.org/en-US/firefox/new/"
    );
    expect(renderedComponent.prop("title")).toEqual(
      "HTMLDocument https://www.mozilla.org/en-US/firefox/new/"
    );
    expectActorAttribute(renderedComponent, stub.actor);
  });

  it("renders location-less document with expected text content", () => {
    const renderedComponent = shallow(
      Document.rep({
        object: stubs.get("Location-less Document"),
      })
    );

    expect(renderedComponent.text()).toEqual("HTMLDocument");
  });
});
