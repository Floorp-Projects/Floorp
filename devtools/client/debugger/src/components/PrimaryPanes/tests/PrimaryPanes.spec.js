/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import React from "react";
import { shallow } from "enzyme";
import { showMenu } from "devtools-contextmenu";

import { copyToTheClipboard } from "../../../utils/clipboard";
import PrimaryPanes from "..";

jest.mock("devtools-contextmenu", () => ({ showMenu: jest.fn() }));
jest.mock("../../../utils/clipboard", () => ({
  copyToTheClipboard: jest.fn(),
}));

describe("PrimaryPanes", () => {
  afterEach(() => {
    (copyToTheClipboard: any).mockClear();
    showMenu.mockClear();
  });

  describe("with custom root", () => {
    it("renders custom root source list", async () => {
      const { component } = render({
        projectRoot: "mdn.com",
      });
      expect(component).toMatchSnapshot();
    });

    it("calls clearProjectDirectoryRoot on click", async () => {
      const { component, props } = render({
        projectRoot: "mdn",
      });
      component.find(".sources-clear-root").simulate("click");
      expect(props.clearProjectDirectoryRoot).toHaveBeenCalled();
    });

    it("renders empty custom root source list", async () => {
      const { component } = render({
        projectRoot: "custom",
        sources: {},
      });
      expect(component).toMatchSnapshot();
    });
  });
});

function generateDefaults(overrides) {
  return {
    horizontal: false,
    projectRoot: "",
    sourceSearchOn: false,
    setPrimaryPaneTab: jest.fn(),
    setActiveSearch: jest.fn(),
    closeActiveSearch: jest.fn(),
    clearProjectDirectoryRoot: jest.fn(),
    threads: [],
    ...overrides,
  };
}

function render(overrides = {}) {
  const props = generateDefaults(overrides);
  // $FlowIgnore
  const component = shallow(<PrimaryPanes.WrappedComponent {...props} />);
  const defaultState = component.state();
  const instance = component.instance();

  instance.shouldComponentUpdate = () => true;

  return { component, props, defaultState, instance };
}
