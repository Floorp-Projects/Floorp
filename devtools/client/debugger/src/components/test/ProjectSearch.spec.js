/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import React from "react";
import { mount, shallow } from "enzyme";
import { ProjectSearch } from "../ProjectSearch";
import { statusType } from "../../reducers/project-text-search";
import { mockcx } from "../../utils/test-mockup";

const hooks = { on: [], off: [] };
const shortcuts = {
  dispatch(eventName) {
    hooks.on.forEach(hook => {
      if (hook.event === eventName) {
        hook.cb();
      }
    });
    hooks.off.forEach(hook => {
      if (hook.event === eventName) {
        hook.cb();
      }
    });
  },
  on: jest.fn((event, cb) => hooks.on.push({ event, cb })),
  off: jest.fn((event, cb) => hooks.off.push({ event, cb })),
};

const context = { shortcuts };

const testResults = [
  {
    filepath: "testFilePath1",
    type: "RESULT",
    matches: [
      {
        match: "match1",
        value: "some thing match1",
        column: 30,
        type: "MATCH",
      },
      {
        match: "match2",
        value: "some thing match2",
        column: 60,
        type: "MATCH",
      },
      {
        match: "match3",
        value: "some thing match3",
        column: 90,
        type: "MATCH",
      },
    ],
  },
  {
    filepath: "testFilePath2",
    type: "RESULT",
    matches: [
      {
        match: "match4",
        value: "some thing match4",
        column: 80,
        type: "MATCH",
      },
      {
        match: "match5",
        value: "some thing match5",
        column: 40,
        type: "MATCH",
      },
    ],
  },
];

const testMatch = {
  type: "MATCH",
  match: "match1",
  value: "some thing match1",
  sourceId: "some-target/source42",
  line: 3,
  column: 30,
};

function render(overrides = {}, mounted = false) {
  const props = {
    cx: mockcx,
    status: "DONE",
    sources: {},
    results: [],
    query: "foo",
    activeSearch: "project",
    closeProjectSearch: jest.fn(),
    searchSources: jest.fn(),
    clearSearch: jest.fn(),
    updateSearchStatus: jest.fn(),
    selectSpecificLocation: jest.fn(),
    doSearchForHighlight: jest.fn(),
    setActiveSearch: jest.fn(),
    ...overrides,
  };

  return mounted
    ? mount(<ProjectSearch {...props} />, { context })
    : shallow(<ProjectSearch {...props} />, { context });
}

describe("ProjectSearch", () => {
  beforeEach(() => {
    context.shortcuts.on.mockClear();
    context.shortcuts.off.mockClear();
  });

  it("renders nothing when disabled", () => {
    const component = render({ activeSearch: "" });
    expect(component).toMatchSnapshot();
  });

  it("where <Enter> has not been pressed", () => {
    const component = render({ query: "" });
    expect(component).toMatchSnapshot();
  });

  it("found no search results", () => {
    const component = render();
    expect(component).toMatchSnapshot();
  });

  it("should display loading message while search is in progress", () => {
    const component = render({
      query: "match",
      status: statusType.fetching,
    });
    expect(component).toMatchSnapshot();
  });

  it("found search results", () => {
    const component = render(
      {
        query: "match",
        results: testResults,
      },
      true
    );
    expect(component).toMatchSnapshot();
  });

  it("turns off shortcuts on unmount", () => {
    const component = render({
      query: "",
    });
    expect(component).toMatchSnapshot();
    component.unmount();
    expect(context.shortcuts.off).toHaveBeenCalled();
  });

  it("calls inputOnChange", () => {
    const component = render(
      {
        results: testResults,
      },
      true
    );
    component
      .find("SearchInput input")
      .simulate("change", { target: { value: "bar" } });
    expect(component.state().inputValue).toEqual("bar");
  });

  it("onKeyDown Escape/Other", () => {
    const searchSources = jest.fn();
    const component = render(
      {
        results: testResults,
        searchSources,
      },
      true
    );
    component.find("SearchInput input").simulate("keydown", { key: "Escape" });
    expect(searchSources).not.toHaveBeenCalled();
    searchSources.mockClear();
    component
      .find("SearchInput input")
      .simulate("keydown", { key: "Other", stopPropagation: jest.fn() });
    expect(searchSources).not.toHaveBeenCalled();
  });

  it("onKeyDown Enter", () => {
    const searchSources = jest.fn();
    const component = render(
      {
        results: testResults,
        searchSources,
      },
      true
    );
    component
      .find("SearchInput input")
      .simulate("keydown", { key: "Enter", stopPropagation: jest.fn() });
    expect(searchSources).toHaveBeenCalledWith(mockcx, "foo");
  });

  it("onEnterPress shortcut no match or setExpanded", () => {
    const selectSpecificLocation = jest.fn();
    const component = render(
      {
        results: testResults,
        selectSpecificLocation,
      },
      true
    );
    component.instance().state.focusedItem = null;
    shortcuts.dispatch("Enter");
    expect(selectSpecificLocation).not.toHaveBeenCalled();
  });

  it("onEnterPress shortcut match", () => {
    const selectSpecificLocation = jest.fn();
    const component = render(
      {
        results: testResults,
        selectSpecificLocation,
      },
      true
    );
    component.instance().state.focusedItem = { ...testMatch };
    shortcuts.dispatch("Enter");
    expect(selectSpecificLocation).toHaveBeenCalledWith(mockcx, {
      sourceId: "some-target/source42",
      line: 3,
      column: 30,
    });
  });

  it("state.inputValue responds to prop.query changes", () => {
    const component = render({ query: "foo" });
    expect(component.state().inputValue).toEqual("foo");
    component.setProps({ query: "" });
    expect(component.state().inputValue).toEqual("");
  });

  describe("showErrorEmoji", () => {
    it("false if not done & results", () => {
      const component = render({
        status: statusType.fetching,
        results: testResults,
      });
      expect(component).toMatchSnapshot();
    });

    it("false if not done & no results", () => {
      const component = render({
        status: statusType.fetching,
      });
      expect(component).toMatchSnapshot();
    });

    // "false if done & has results"
    // is the same test as "found search results"

    // "true if done & has no results"
    // is the same test as "found no search results"
  });
});
