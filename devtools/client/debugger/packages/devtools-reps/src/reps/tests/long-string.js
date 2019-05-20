/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const { shallow } = require("enzyme");
const { REPS, getRep } = require("../rep");

const { ELLIPSIS } = require("../rep-utils");

const { expectActorAttribute } = require("./test-helpers");

const { StringRep } = REPS;
const stubs = require("../stubs/long-string");

function quoteNewlines(text) {
  return text.split("\n").join("\\n");
}

describe("long StringRep", () => {
  it("selects String Rep", () => {
    const stub = stubs.get("testMultiline");

    expect(getRep(stub)).toEqual(StringRep.rep);
  });

  it("renders with expected text content for multiline string", () => {
    const stub = stubs.get("testMultiline");
    const renderedComponent = shallow(
      StringRep.rep({
        object: stub,
      })
    );

    expect(renderedComponent.text()).toEqual(
      quoteNewlines(`"${stub.initial}${ELLIPSIS}"`)
    );
    expectActorAttribute(renderedComponent, stub.actor);
  });

  it(
    "renders with expected text content for multiline string with " +
      "specified number of characters",
    () => {
      const stub = stubs.get("testMultiline");
      const renderedComponent = shallow(
        StringRep.rep({
          object: stub,
          cropLimit: 20,
        })
      );

      expect(renderedComponent.text()).toEqual(
        `"a\\naaaaaaaaaaaaaaaaaa${ELLIPSIS}"`
      );
    }
  );

  it("renders with expected text for multiline string when open", () => {
    const stub = stubs.get("testMultiline");
    const renderedComponent = shallow(
      StringRep.rep({
        object: stub,
        member: { open: true },
        cropLimit: 20,
      })
    );

    expect(renderedComponent.text()).toEqual(
      quoteNewlines(`"${stub.initial}${ELLIPSIS}"`)
    );
  });

  it(
    "renders with expected text content when grip has a fullText" +
      "property and is open",
    () => {
      const stub = stubs.get("testLoadedFullText");
      const renderedComponent = shallow(
        StringRep.rep({
          object: stub,
          member: { open: true },
          cropLimit: 20,
        })
      );

      expect(renderedComponent.text()).toEqual(
        quoteNewlines(`"${stub.fullText}"`)
      );
    }
  );

  it(
    "renders with expected text content when grip has a fullText " +
      "property and is not open",
    () => {
      const stub = stubs.get("testLoadedFullText");
      const renderedComponent = shallow(
        StringRep.rep({
          object: stub,
          cropLimit: 20,
        })
      );

      expect(renderedComponent.text()).toEqual(
        `"a\\naaaaaaaaaaaaaaaaaa${ELLIPSIS}"`
      );
    }
  );

  it("expected to omit quotes", () => {
    const stub = stubs.get("testMultiline");
    const renderedComponent = shallow(
      StringRep.rep({
        object: stub,
        cropLimit: 20,
        useQuotes: false,
      })
    );

    expect(renderedComponent.html()).toEqual(
      '<span data-link-actor-id="server1.conn1.child1/longString58" ' +
        `class="objectBox objectBox-string">a\naaaaaaaaaaaaaaaaaa${ELLIPSIS}` +
        "</span>"
    );
  });
});
