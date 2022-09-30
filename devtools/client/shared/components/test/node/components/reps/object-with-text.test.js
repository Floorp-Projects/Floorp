/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

const {
  REPS,
  getRep,
} = require("resource://devtools/client/shared/components/reps/reps/rep.js");

const { shallow } = require("enzyme");
const {
  expectActorAttribute,
} = require("resource://devtools/client/shared/components/test/node/components/reps/test-helpers.js");

const stubs = require("resource://devtools/client/shared/components/test/node/stubs/reps/object-with-text.js");
const { ObjectWithText, Rep } = REPS;

describe("Object with text - CSSStyleRule", () => {
  const gripStub = stubs.get("ShadowRule");

  // Test that correct rep is chosen
  it("selects ObjectsWithText Rep", () => {
    expect(getRep(gripStub)).toEqual(ObjectWithText.rep);
  });

  // Test rendering
  it("renders with the correct text content", () => {
    const renderedComponent = shallow(
      Rep({
        object: gripStub,
        shouldRenderTooltip: true,
      })
    );

    expect(renderedComponent.text()).toEqual('CSSStyleRule ".Shadow"');
    expect(renderedComponent.prop("title")).toEqual('CSSStyleRule ".Shadow"');
    expectActorAttribute(renderedComponent, gripStub.actor);
  });
});

describe("Object with text - CSSMediaRule", () => {
  const gripStub = stubs.get("CSSMediaRule");

  // Test that correct rep is chosen
  it("selects ObjectsWithText Rep", () => {
    expect(getRep(gripStub)).toEqual(ObjectWithText.rep);
  });

  // Test rendering
  it("renders with the correct text content", () => {
    const renderedComponent = shallow(
      Rep({
        object: gripStub,
        shouldRenderTooltip: true,
      })
    );

    const text =
      'CSSMediaRule "(min-height: 680px), screen and (orientation: portrait)"';
    expect(renderedComponent.text()).toEqual(text);
    expect(renderedComponent.prop("title")).toEqual(text);
    expectActorAttribute(renderedComponent, gripStub.actor);
  });
});
