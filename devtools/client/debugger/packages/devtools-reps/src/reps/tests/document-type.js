/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const { shallow } = require("enzyme");
const { REPS, getRep } = require("../rep");

const { expectActorAttribute } = require("./test-helpers");

const { DocumentType } = REPS;
const stubs = require("../stubs/document-type");

describe("DocumentType", () => {
  const stub = stubs.get("html");
  it("correctly selects DocumentType Rep", () => {
    expect(getRep(stub)).toBe(DocumentType.rep);
  });

  it("renders with expected text content on html doctype", () => {
    const renderedComponent = shallow(
      DocumentType.rep({
        object: stub,
      })
    );

    expect(renderedComponent.text()).toEqual("<!DOCTYPE html>");
    expectActorAttribute(renderedComponent, stub.actor);
  });

  it("renders with expected text content on empty doctype", () => {
    const unnamedStub = stubs.get("unnamed");
    const renderedComponent = shallow(
      DocumentType.rep({
        object: unnamedStub,
      })
    );
    expect(renderedComponent.text()).toEqual("<!DOCTYPE>");
    expectActorAttribute(renderedComponent, unnamedStub.actor);
  });
});
