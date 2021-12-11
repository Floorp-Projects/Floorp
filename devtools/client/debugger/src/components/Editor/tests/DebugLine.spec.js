/* eslint max-nested-callbacks: ["error", 7] */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React from "react";
import { shallow } from "enzyme";

import DebugLine from "../DebugLine";

import * as asyncValue from "../../../utils/async-value";
import { createSourceObject } from "../../../utils/test-head";
import { setDocument, toEditorLine } from "../../../utils/editor";

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
    source: {
      ...createSourceObject("foo"),
      content: null,
    },
    ...overrides,
  };
}

function createLocation(line) {
  return {
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
  setDocument(props.source.id, doc);

  const component = shallow(<DebugLine.WrappedComponent {...props} />, {
    lifecycleExperimental: true,
  });
  return { component, props, clear, editor, doc };
}

describe("DebugLine Component", () => {
  describe("pausing at the first location", () => {
    it("should show a new debug line", async () => {
      const { component, props, doc } = render({
        source: {
          ...createSourceObject("foo"),
          content: asyncValue.fulfilled({
            type: "text",
            value: "",
            contentType: undefined,
          }),
        },
      });
      const line = 2;
      const location = createLocation(line);

      component.setProps({ ...props, location });

      expect(doc.removeLineClass.mock.calls).toEqual([]);
      expect(doc.addLineClass.mock.calls).toEqual([
        [toEditorLine("foo", line), "wrapClass", "new-debug-line"],
      ]);
    });

    describe("pausing at a new location", () => {
      it("should replace the first debug line", async () => {
        const { props, component, clear, doc } = render({
          source: {
            ...createSourceObject("foo"),
            content: asyncValue.fulfilled({
              type: "text",
              value: "",
              contentType: undefined,
            }),
          },
        });

        component.instance().debugExpression = { clear: jest.fn() };
        const firstLine = 2;
        const secondLine = 2;

        component.setProps({ ...props, location: createLocation(firstLine) });
        component.setProps({
          ...props,
          frame: createLocation(secondLine),
        });

        expect(doc.removeLineClass.mock.calls).toEqual([
          [toEditorLine("foo", firstLine), "wrapClass", "new-debug-line"],
        ]);

        expect(doc.addLineClass.mock.calls).toEqual([
          [toEditorLine("foo", firstLine), "wrapClass", "new-debug-line"],
          [toEditorLine("foo", secondLine), "wrapClass", "new-debug-line"],
        ]);

        expect(doc.markText.mock.calls).toEqual([
          [
            { ch: 2, line: toEditorLine("foo", firstLine) },
            { ch: null, line: toEditorLine("foo", firstLine) },
            { className: "debug-expression to-line-end" },
          ],
          [
            { ch: 2, line: toEditorLine("foo", secondLine) },
            { ch: null, line: toEditorLine("foo", secondLine) },
            { className: "debug-expression to-line-end" },
          ],
        ]);

        expect(clear.mock.calls).toEqual([[]]);
      });
    });

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
