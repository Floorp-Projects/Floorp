/* eslint max-nested-callbacks: ["error", 7] */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React from "react";
import { shallow } from "enzyme";

import DebugLine from "../DebugLine";

import { setDocument } from "../../../utils/editor";

function createMockDocument(clear) {
  const doc = {
    addLineClass: jest.fn(),
    removeLineClass: jest.fn(),
    markText: jest.fn(() => ({ clear })),
    getLine: line => "",
  };

  return doc;
}

function generateDefaults(editor, overrides) {
  return {
    editor,
    pauseInfo: {
      why: { type: "breakpoint" },
    },
    frame: null,
    sourceTextContent: null,
    ...overrides,
  };
}

function createLocation(line) {
  return {
    source: {
      id: "foo",
    },
    sourceId: "foo",
    line,
    column: 2,
  };
}

function render(overrides = {}) {
  const clear = jest.fn();
  const editor = { codeMirror: {} };
  const props = generateDefaults(editor, overrides);

  const doc = createMockDocument(clear);
  setDocument("foo", doc);

  const component = shallow(
    React.createElement(DebugLine.WrappedComponent, props),
    {
      lifecycleExperimental: true,
    }
  );
  return { component, props, clear, editor, doc };
}

describe("DebugLine Component", () => {
  describe("pausing at the first location", () => {
    describe("when there is no selected frame", () => {
      it("should not set the debug line", () => {
        const { component, props, doc } = render({ frame: null });
        const line = 2;
        const location = createLocation(line);

        component.setProps({ ...props, location });
        expect(doc.removeLineClass).not.toHaveBeenCalled();
      });
    });

    describe("when there is a different source", () => {
      it("should not set the debug line", async () => {
        const { component, doc } = render();
        const newSelectedFrame = { location: { sourceId: "bar" } };
        expect(doc.removeLineClass).not.toHaveBeenCalled();

        component.setProps({ frame: newSelectedFrame });
        expect(doc.removeLineClass).not.toHaveBeenCalled();
      });
    });
  });
});
