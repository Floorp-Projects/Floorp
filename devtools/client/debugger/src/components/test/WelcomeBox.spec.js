/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React from "react";
import { shallow } from "enzyme";

import { WelcomeBox } from "../WelcomeBox";

function render(overrides = {}) {
  const props = {
    horizontal: false,
    togglePaneCollapse: jest.fn(),
    endPanelCollapsed: false,
    setActiveSearch: jest.fn(),
    openQuickOpen: jest.fn(),
    toggleShortcutsModal: jest.fn(),
    setPrimaryPaneTab: jest.fn(),
    ...overrides,
  };
  const component = shallow(React.createElement(WelcomeBox, props));
  return { component, props };
}

describe("WelomeBox", () => {
  it("renders with default values", () => {
    const { component } = render();
    expect(component).toMatchSnapshot();
  });

  it("doesn't render toggle button in horizontal mode", () => {
    const { component } = render({
      horizontal: true,
    });
    expect(component.find("PaneToggleButton")).toHaveLength(0);
  });

  it("calls correct function on searchSources click", () => {
    const { component, props } = render();

    component.find(".welcomebox__searchSources").simulate("click");
    expect(props.openQuickOpen).toHaveBeenCalled();
  });

  it("calls correct function on searchProject click", () => {
    const { component, props } = render();

    component.find(".welcomebox__searchProject").simulate("click");
    expect(props.setActiveSearch).toHaveBeenCalled();
  });

  it("calls correct function on allShotcuts click", () => {
    const { component, props } = render();

    component.find(".welcomebox__allShortcuts").simulate("click");
    expect(props.toggleShortcutsModal).toHaveBeenCalled();
  });
});
