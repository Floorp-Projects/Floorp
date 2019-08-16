/* eslint max-nested-callbacks: ["error", 7] */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import React from "react";
import { shallow } from "enzyme";

import SourceFooter from "../Footer";
import { createSourceObject } from "../../../utils/test-head";
import { setDocument } from "../../../utils/editor";

function createMockDocument(clear, position) {
  const doc = {
    getCursor: jest.fn(() => position),
  };
  return doc;
}

function generateDefaults(overrides) {
  return {
    editor: {
      codeMirror: {
        doc: {},
        cursorActivity: jest.fn(),
        on: jest.fn(),
      },
    },
    endPanelCollapsed: false,
    selectedSource: {
      ...createSourceObject("foo"),
      content: null,
    },
    ...overrides,
  };
}

function render(overrides = {}, position = { line: 0, column: 0 }) {
  const clear = jest.fn();
  const props = generateDefaults(overrides);

  const doc = createMockDocument(clear, position);
  setDocument(props.selectedSource.id, doc);

  // $FlowIgnore
  const component = shallow(<SourceFooter.WrappedComponent {...props} />, {
    lifecycleExperimental: true,
  });
  return { component, props, clear, doc };
}

describe("SourceFooter Component", () => {
  describe("default case", () => {
    it("should render", () => {
      const { component } = render();
      expect(component).toMatchSnapshot();
    });
  });

  describe("move cursor", () => {
    it("should render new cursor position", () => {
      const { component } = render();
      component.setState({ cursorPosition: { line: 5, column: 10 } });

      expect(component).toMatchSnapshot();
    });
  });
});
