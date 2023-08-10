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

const {
  MODE,
} = require("resource://devtools/client/shared/components/reps/reps/constants.js");
const { Rep, CommentNode } = REPS;
const stubs = require("resource://devtools/client/shared/components/test/node/stubs/reps/comment-node.js");

describe("CommentNode", () => {
  const stub = stubs.get("Comment")._grip;

  it("selects CommentNode Rep correctly", () => {
    expect(getRep(stub)).toEqual(CommentNode.rep);
  });

  it("renders with correct class names", () => {
    const renderedComponent = shallow(
      Rep({
        object: stub,
      })
    );

    expect(renderedComponent.hasClass("objectBox theme-comment")).toBe(true);
  });

  it("renders with correct title tooltip", () => {
    const renderedComponent = shallow(
      Rep({
        object: stub,
        shouldRenderTooltip: true,
      })
    );

    expect(renderedComponent.prop("title")).toBe(
      "<!-- test\nand test\nand test\nand test\nand test\nand test\nand test -->"
    );
  });

  it("renders as expected", () => {
    const object = stubs.get("Comment")._grip;
    const renderRep = props => shallow(CommentNode.rep({ object, ...props }));

    let component = renderRep({ mode: undefined });
    expect(component.text()).toEqual(
      "<!-- test\nand test\nand test\nan…d test\nand test\nand test -->"
    );
    expectActorAttribute(component, object.actor);

    component = renderRep({ mode: MODE.TINY });
    expect(component.text()).toEqual(
      "<!-- test\\nand test\\na… test\\nand test -->"
    );
    expectActorAttribute(component, object.actor);

    component = renderRep({ mode: MODE.HEADER });
    expect(component.text()).toEqual(
      "<!-- test\\nand test\\na… test\\nand test -->"
    );
    expectActorAttribute(component, object.actor);

    component = renderRep({ mode: MODE.LONG });
    expect(component.text()).toEqual(`<!-- ${stub.preview.textContent} -->`);
    expectActorAttribute(component, object.actor);
  });
});
