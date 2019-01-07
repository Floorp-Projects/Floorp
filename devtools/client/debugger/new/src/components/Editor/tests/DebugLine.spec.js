/* eslint max-nested-callbacks: ["error", 7] */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React from "react";
import { shallow } from "enzyme";

import DebugLine from "../DebugLine";

import { makeSource } from "../../../utils/test-head";
import { setDocument, toEditorLine } from "../../../utils/editor";

function createMockDocument(clear) {
  const doc = {
    addLineClass: jest.fn(),
    removeLineClass: jest.fn(),
    markText: jest.fn(() => ({ clear })),
    getLine: line => ""
  };

  return doc;
}

function generateDefaults(editor, overrides) {
  return {
    editor,
    pauseInfo: {
      why: { type: "breakpoint" }
    },
    selectedFrame: null,
    selectedSource: makeSource("foo"),
    ...overrides
  };
}

function createFrame(line) {
  return {
    location: {
      sourceId: "foo",
      line,
      column: 2
    }
  };
}

function render(overrides = {}) {
  const clear = jest.fn();
  const editor = { codeMirror: {} };
  const props = generateDefaults(editor, overrides);

  const doc = createMockDocument(clear);
  setDocument(props.selectedSource.id, doc);

  const component = shallow(<DebugLine.WrappedComponent {...props} />, {
    lifecycleExperimental: true
  });
  return { component, props, clear, editor, doc };
}

describe("DebugLine Component", () => {
  describe("pausing at the first location", () => {
    it("should show a new debug line", async () => {
      const { component, props, doc } = render({
        selectedSource: makeSource("foo", { loadedState: "loaded" })
      });
      const line = 2;
      const selectedFrame = createFrame(line);

      component.setProps({ ...props, selectedFrame });

      expect(doc.removeLineClass.mock.calls).toEqual([]);
      expect(doc.addLineClass.mock.calls).toEqual([
        [toEditorLine(line), "line", "new-debug-line"]
      ]);
    });

    describe("pausing at a new location", () => {
      it("should replace the first debug line", async () => {
        const { props, component, clear, doc } = render({
          selectedSource: makeSource("foo", { loadedState: "loaded" })
        });

        component.instance().debugExpression = { clear: jest.fn() };
        const firstLine = 2;
        const secondLine = 2;

        component.setProps({ ...props, selectedFrame: createFrame(firstLine) });
        component.setProps({
          ...props,
          selectedFrame: createFrame(secondLine)
        });

        expect(doc.removeLineClass.mock.calls).toEqual([
          [toEditorLine(firstLine), "line", "new-debug-line"]
        ]);

        expect(doc.addLineClass.mock.calls).toEqual([
          [toEditorLine(firstLine), "line", "new-debug-line"],
          [toEditorLine(secondLine), "line", "new-debug-line"]
        ]);

        expect(doc.markText.mock.calls).toEqual([
          [
            { ch: 2, line: toEditorLine(firstLine) },
            { ch: null, line: toEditorLine(firstLine) },
            { className: "debug-expression" }
          ],
          [
            { ch: 2, line: toEditorLine(secondLine) },
            { ch: null, line: toEditorLine(secondLine) },
            { className: "debug-expression" }
          ]
        ]);

        expect(clear.mock.calls).toEqual([[]]);
      });
    });

    describe("when there is no selected frame", () => {
      it("should not set the debug line", () => {
        const { component, props, doc } = render({ selectedFrame: null });
        const line = 2;
        const selectedFrame = createFrame(line);

        component.setProps({ ...props, selectedFrame });
        expect(doc.removeLineClass).not.toHaveBeenCalled();
      });
    });

    describe("when there is a different source", () => {
      it("should not set the debug line", async () => {
        const { component, doc } = render();
        const newSelectedFrame = { location: { sourceId: "bar" } };
        expect(doc.removeLineClass).not.toHaveBeenCalled();

        component.setProps({ selectedFrame: newSelectedFrame });
        expect(doc.removeLineClass).not.toHaveBeenCalled();
      });
    });
  });
});
