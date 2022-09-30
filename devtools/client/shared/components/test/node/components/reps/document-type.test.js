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

const { DocumentType } = REPS;
const stubs = require("resource://devtools/client/shared/components/test/node/stubs/reps/document-type.js");

describe("DocumentType", () => {
  const stub = stubs.get("html");
  it("correctly selects DocumentType Rep", () => {
    expect(getRep(stub)).toBe(DocumentType.rep);
  });

  it("renders with expected text content on html doctype", () => {
    const renderedComponent = shallow(
      DocumentType.rep({
        object: stub,
        shouldRenderTooltip: true,
      })
    );

    expect(renderedComponent.text()).toEqual("<!DOCTYPE html>");
    expect(renderedComponent.prop("title")).toEqual("<!DOCTYPE html>");
    expectActorAttribute(renderedComponent, stub.actor);
  });

  it("renders with expected text content on empty doctype", () => {
    const unnamedStub = stubs.get("unnamed");
    const renderedComponent = shallow(
      DocumentType.rep({
        object: unnamedStub,
        shouldRenderTooltip: true,
      })
    );
    expect(renderedComponent.text()).toEqual("<!DOCTYPE>");
    expect(renderedComponent.prop("title")).toEqual("<!DOCTYPE>");
    expectActorAttribute(renderedComponent, unnamedStub.actor);
  });
});
