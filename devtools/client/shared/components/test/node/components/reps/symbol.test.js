/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

const { shallow } = require("enzyme");
const { REPS } = require("devtools/client/shared/components/reps/reps/rep");
const { Rep } = REPS;
const stubs = require("devtools/client/shared/components/test/node/stubs/reps/symbol");
const {
  expectActorAttribute,
} = require("devtools/client/shared/components/test/node/components/reps/test-helpers");

describe("test Symbol", () => {
  const stub = stubs.get("Symbol");

  it("renders with the expected content", () => {
    const renderedComponent = shallow(
      Rep({
        object: stub,
        shouldRenderTooltip: true,
      })
    );

    expect(renderedComponent.text()).toEqual('Symbol("foo")');
    expect(renderedComponent.prop("title")).toBe("Symbol(foo)");
    expectActorAttribute(renderedComponent, stub.actor);
  });
});

describe("test Symbol without identifier", () => {
  const stub = stubs.get("SymbolWithoutIdentifier");

  it("renders the expected content", () => {
    const renderedComponent = shallow(
      Rep({
        object: stub,
      })
    );

    expect(renderedComponent.text()).toEqual("Symbol()");
    expectActorAttribute(renderedComponent, stub.actor);
  });
});

describe("test Symbol with long string", () => {
  const stub = stubs.get("SymbolWithLongString");

  it("renders the expected content", () => {
    const renderedComponent = shallow(
      Rep({
        object: stub,
      })
    );

    expect(renderedComponent.text()).toEqual(
      'Symbol("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa…")'
    );
    expectActorAttribute(renderedComponent, stub.actor);
  });
});
