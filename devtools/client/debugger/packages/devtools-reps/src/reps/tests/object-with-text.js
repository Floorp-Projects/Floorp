/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const { REPS, getRep } = require("../rep");

const { shallow } = require("enzyme");
const { expectActorAttribute } = require("./test-helpers");

const stubs = require("../stubs/object-with-text");
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
      })
    );

    expect(renderedComponent.text()).toEqual('CSSStyleRule ".Shadow"');
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
      })
    );

    const text =
      'CSSMediaRule "(min-height: 680px), screen and (orientation: portrait)"';
    expect(renderedComponent.text()).toEqual(text);
    expectActorAttribute(renderedComponent, gripStub.actor);
  });
});
