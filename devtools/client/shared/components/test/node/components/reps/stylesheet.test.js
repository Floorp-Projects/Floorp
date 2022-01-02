/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

const { shallow } = require("enzyme");
const {
  REPS,
  getRep,
} = require("devtools/client/shared/components/reps/reps/rep");
const { StyleSheet, Rep } = REPS;
const stubs = require("devtools/client/shared/components/test/node/stubs/reps/stylesheet");
const {
  expectActorAttribute,
} = require("devtools/client/shared/components/test/node/components/reps/test-helpers");

describe("Test StyleSheet", () => {
  const stub = stubs.get("StyleSheet");

  it("selects the StyleSheet Rep", () => {
    expect(getRep(stub)).toEqual(StyleSheet.rep);
  });

  it("renders with the expected text content", () => {
    const renderedComponent = shallow(
      Rep({
        object: stub,
        shouldRenderTooltip: true,
      })
    );

    expect(renderedComponent.text()).toEqual(
      "CSSStyleSheet https://example.com/styles.css"
    );
    expect(renderedComponent.prop("title")).toEqual(
      "CSSStyleSheet https://example.com/styles.css"
    );
    expectActorAttribute(renderedComponent, stub.actor);
  });
});
