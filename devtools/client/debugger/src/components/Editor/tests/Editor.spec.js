/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import React from "react";
import { shallow } from "enzyme";
import Editor from "../index";
import type { Source, SourceWithContent } from "../../../types";
import { getDocument } from "../../../utils/editor/source-documents";
import * as asyncValue from "../../../utils/async-value";

function generateDefaults(overrides) {
  return {
    toggleBreakpoint: jest.fn(),
    updateViewport: jest.fn(),
    toggleDisabledBreakpoint: jest.fn(),
    ...overrides,
  };
}

function createMockEditor() {
  return {
    codeMirror: {
      doc: {},
      getOption: jest.fn(),
      setOption: jest.fn(),
      scrollTo: jest.fn(),
      charCoords: ({ line, ch }) => ({ top: line, left: ch }),
      getScrollerElement: () => ({ offsetWidth: 0, offsetHeight: 0 }),
      getScrollInfo: () => ({
        top: 0,
        left: 0,
        clientWidth: 0,
        clientHeight: 0,
      }),
      defaultCharWidth: () => 0,
      defaultTextHeight: () => 0,
      display: { gutters: { querySelector: jest.fn() } },
    },
    setText: jest.fn(),
    on: jest.fn(),
    off: jest.fn(),
    createDocument: () => {
      let val;
      return {
        getLine: line => "",
        getValue: () => val,
        setValue: newVal => (val = newVal),
      };
    },
    replaceDocument: jest.fn(),
    setMode: jest.fn(),
  };
}

function createMockSourceWithContent(
  overrides: $Shape<
    Source & {
      loadedState: "loaded" | "loading" | "unloaded",
      text: string,
      contentType: ?string,
      error: string,
      isWasm: boolean,
    }
  >
): SourceWithContent {
  const {
    loadedState = "loaded",
    text = "the text",
    contentType = undefined,
    error = undefined,
    ...otherOverrides
  } = overrides;

  const source: Source = ({
    id: "foo",
    url: "foo",
    ...otherOverrides,
  }: any);
  let content = null;
  if (loadedState === "loaded") {
    if (typeof text !== "string") {
      throw new Error("Cannot create a non-text source");
    }

    content = error
      ? asyncValue.rejected(error)
      : asyncValue.fulfilled({
          type: "text",
          value: text,
          contentType: contentType || undefined,
        });
  }

  return {
    source,
    content,
  };
}

function render(overrides = {}) {
  const props = generateDefaults(overrides);
  const mockEditor = createMockEditor();

  // $FlowIgnore
  const component = shallow(<Editor.WrappedComponent {...props} />, {
    context: {
      shortcuts: { on: jest.fn() },
    },
    disableLifecycleMethods: true,
  });

  return { component, props, mockEditor };
}

describe("Editor", () => {
  describe("When empty", () => {
    it("should render", async () => {
      const { component } = render();
      expect(component).toMatchSnapshot();
    });
  });

  describe("When loading initial source", () => {
    it("should show a loading message", async () => {
      const { component, mockEditor } = render();
      await component.setState({ editor: mockEditor });
      component.setProps({
        selectedSourceWithContent: {
          source: { loadedState: "loading" },
          content: null,
        },
      });

      expect(mockEditor.replaceDocument.mock.calls[0][0].getValue()).toBe(
        "Loading…"
      );
      expect(mockEditor.codeMirror.scrollTo.mock.calls).toEqual([]);
    });
  });

  describe("When loaded", () => {
    it("should show text", async () => {
      const { component, mockEditor, props } = render({});

      await component.setState({ editor: mockEditor });
      await component.setProps({
        ...props,
        selectedSourceWithContent: createMockSourceWithContent({
          loadedState: "loaded",
        }),
        selectedLocation: { sourceId: "foo", line: 3, column: 1 },
      });

      expect(mockEditor.setText.mock.calls).toEqual([["the text"]]);
      expect(mockEditor.codeMirror.scrollTo.mock.calls).toEqual([[1, 2]]);
    });
  });

  describe("When error", () => {
    it("should show error text", async () => {
      const { component, mockEditor, props } = render({});

      await component.setState({ editor: mockEditor });
      await component.setProps({
        ...props,
        selectedSourceWithContent: createMockSourceWithContent({
          loadedState: "loaded",
          text: undefined,
          error: "error text",
        }),
        selectedLocation: { sourceId: "bad-foo", line: 3, column: 1 },
      });

      expect(mockEditor.setText.mock.calls).toEqual([
        ["Error loading this URI: error text"],
      ]);
    });

    it("should show wasm error", async () => {
      const { component, mockEditor, props } = render({});

      await component.setState({ editor: mockEditor });
      await component.setProps({
        ...props,
        selectedSourceWithContent: createMockSourceWithContent({
          loadedState: "loaded",
          isWasm: true,
          text: undefined,
          error: "blah WebAssembly binary source is not available blah",
        }),
        selectedLocation: { sourceId: "bad-foo", line: 3, column: 1 },
      });

      expect(mockEditor.setText.mock.calls).toEqual([
        ["Please refresh to debug this module"],
      ]);
    });
  });

  describe("When navigating to a loading source", () => {
    it("should show loading message and not scroll", async () => {
      const { component, mockEditor, props } = render({});

      await component.setState({ editor: mockEditor });
      await component.setProps({
        ...props,
        selectedSourceWithContent: createMockSourceWithContent({
          loadedState: "loaded",
        }),
        selectedLocation: { sourceId: "foo", line: 3, column: 1 },
      });

      // navigate to a new source that is still loading
      await component.setProps({
        ...props,
        selectedSourceWithContent: createMockSourceWithContent({
          id: "bar",
          loadedState: "loading",
        }),
        selectedLocation: { sourceId: "bar", line: 1, column: 1 },
      });

      expect(mockEditor.replaceDocument.mock.calls[1][0].getValue()).toBe(
        "Loading…"
      );

      expect(mockEditor.setText.mock.calls).toEqual([["the text"]]);

      expect(mockEditor.codeMirror.scrollTo.mock.calls).toEqual([[1, 2]]);
    });

    it("should set the mode when symbols load", async () => {
      const { component, mockEditor, props } = render({});

      await component.setState({ editor: mockEditor });

      const selectedSourceWithContent = createMockSourceWithContent({
        loadedState: "loaded",
        contentType: "javascript",
      });

      await component.setProps({ ...props, selectedSourceWithContent });

      const symbols = { hasJsx: true };
      await component.setProps({
        ...props,
        selectedSourceWithContent,
        symbols,
      });

      expect(mockEditor.setMode.mock.calls).toEqual([
        [{ name: "javascript" }],
        [{ name: "jsx" }],
      ]);
    });

    it("should not re-set the mode when the location changes", async () => {
      const { component, mockEditor, props } = render({});

      await component.setState({ editor: mockEditor });

      const selectedSourceWithContent = createMockSourceWithContent({
        loadedState: "loaded",
        contentType: "javascript",
      });

      await component.setProps({ ...props, selectedSourceWithContent });

      // symbols are parsed
      const symbols = { hasJsx: true };
      await component.setProps({
        ...props,
        selectedSourceWithContent,
        symbols,
      });

      // selectedLocation changes e.g. pausing/stepping
      mockEditor.codeMirror.doc = getDocument(
        selectedSourceWithContent.source.id
      );
      mockEditor.codeMirror.getOption = () => ({ name: "jsx" });
      const selectedLocation = { sourceId: "foo", line: 4, column: 1 };

      await component.setProps({
        ...props,
        selectedSourceWithContent,
        symbols,
        selectedLocation,
      });

      expect(mockEditor.setMode.mock.calls).toEqual([
        [{ name: "javascript" }],
        [{ name: "jsx" }],
      ]);
    });
  });

  describe("When navigating to a loaded source", () => {
    it("should show text and then scroll", async () => {
      const { component, mockEditor, props } = render({});

      await component.setState({ editor: mockEditor });
      await component.setProps({
        ...props,
        selectedSourceWithContent: createMockSourceWithContent({
          loadedState: "loading",
        }),
        selectedLocation: { sourceId: "foo", line: 1, column: 1 },
      });

      // navigate to a new source that is still loading
      await component.setProps({
        ...props,
        selectedSourceWithContent: createMockSourceWithContent({
          loadedState: "loaded",
        }),
        selectedLocation: { sourceId: "foo", line: 1, column: 1 },
      });

      expect(mockEditor.replaceDocument.mock.calls[0][0].getValue()).toBe(
        "Loading…"
      );

      expect(mockEditor.setText.mock.calls).toEqual([["the text"]]);

      expect(mockEditor.codeMirror.scrollTo.mock.calls).toEqual([[1, 0]]);
    });
  });
});
