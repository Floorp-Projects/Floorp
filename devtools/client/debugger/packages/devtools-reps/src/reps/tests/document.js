/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const { shallow } = require("enzyme");
const { REPS, getRep } = require("../rep");

const { expectActorAttribute } = require("./test-helpers");

const { Document } = REPS;
const stubs = require("../stubs/document");

describe("Document", () => {
  const stub = stubs.get("Document");
  it("correctly selects Document Rep", () => {
    expect(getRep(stub)).toBe(Document.rep);
  });

  it("renders with expected text content", () => {
    const renderedComponent = shallow(
      Document.rep({
        object: stub,
      })
    );

    expect(renderedComponent.text()).toEqual(
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

describe("XULDocument", () => {
  const stub = stubs.get("XULDocument");
  it("correctly selects Document Rep", () => {
    expect(getRep(stub)).toBe(Document.rep);
  });

  it("renders with expected text content", () => {
    const renderedComponent = shallow(
      Document.rep({
        object: stub,
      })
    );

    expect(renderedComponent.text()).toEqual(
      "XULDocument chrome://browser/content/browser.xul"
    );
    expectActorAttribute(renderedComponent, stub.actor);
  });
});
