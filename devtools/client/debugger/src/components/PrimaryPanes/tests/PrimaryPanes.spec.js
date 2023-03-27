/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React from "react";
import { shallow } from "enzyme";

import PrimaryPanes from "..";

describe("PrimaryPanes", () => {
  it("should not render the directory root header if the directory root has not been set.", () => {
    const { component } = render();
    expect(component).toMatchSnapshot();
  });

  it("should render the directory root header if the directory root has been set.", () => {
    const { component } = render({
      projectRootName: "mdn.com",
    });
    expect(component).toMatchSnapshot();
  });

  it("should call clearProjectDirectoryRoot if .sources-clear-root has been clicked.", () => {
    const { component, props } = render({
      projectRootName: "mdn.com",
    });
    component.find(".sources-clear-root").simulate("click");
    expect(props.clearProjectDirectoryRoot).toHaveBeenCalled();
  });
});

function generateDefaults(overrides) {
  return {
    horizontal: false,
    projectRootName: "",
    sourceSearchOn: false,
    setPrimaryPaneTab: jest.fn(),
    setActiveSearch: jest.fn(),
    closeActiveSearch: jest.fn(),
    clearProjectDirectoryRoot: jest.fn(),
    selectedTab: "sources",
    cx: {},
    threads: [],
    ...overrides,
  };
}

function render(overrides = {}) {
  const props = generateDefaults(overrides);
  const component = shallow(<PrimaryPanes.WrappedComponent {...props} />);
  const defaultState = component.state();
  const instance = component.instance();

  instance.shouldComponentUpdate = () => true;

  return { component, props, defaultState, instance };
}
