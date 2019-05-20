/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import React from "react";
import { shallow } from "enzyme";
import SearchBar from "../SearchBar";
import "../../../workers/search";
import "../../../utils/editor";

// $FlowIgnore
const SearchBarComponent = SearchBar.WrappedComponent;

jest.mock("../../../workers/search", () => ({
  getMatches: () => Promise.resolve(["result"]),
}));

jest.mock("../../../utils/editor", () => ({
  find: () => ({ ch: "1", line: "1" }),
}));

function generateDefaults() {
  return {
    query: "",
    searchOn: true,
    symbolSearchOn: true,
    editor: {},
    searchResults: {},
    selectedSymbolType: "functions",
    selectedSource: {
      text: " text text query text",
    },
    selectedContentLoaded: true,
    setFileSearchQuery: msg => msg,
    symbolSearchResults: [],
    modifiers: {
      get: jest.fn(),
      toJS: () => ({
        caseSensitive: true,
        wholeWord: false,
        regexMatch: false,
      }),
    },
    selectedResultIndex: 0,
    updateSearchResults: jest.fn(),
    doSearch: jest.fn(),
  };
}

function render(overrides = {}) {
  const defaults = generateDefaults();
  const props = { ...defaults, ...overrides };
  const component = shallow(<SearchBarComponent {...props} />, {
    disableLifecycleMethods: true,
  });
  return { component, props };
}

describe("SearchBar", () => {
  it("should render", () => {
    const { component } = render();
    expect(component).toMatchSnapshot();
  });
});

describe("doSearch", () => {
  it("should complete a search", async () => {
    const { component, props } = render();
    component
      .find("SearchInput")
      .simulate("change", { target: { value: "query" } });

    const doSearchArgs = props.doSearch.mock.calls[0][1];
    expect(doSearchArgs).toMatchSnapshot();
  });
});

describe("showErrorEmoji", () => {
  it("true if query + no results", () => {
    const { component } = render({
      query: "test",
      searchResults: {
        count: 0,
      },
    });
    expect(component).toMatchSnapshot();
  });

  it("false if no query + no results", () => {
    const { component } = render({
      query: "",
      searchResults: {
        count: 0,
      },
    });
    expect(component).toMatchSnapshot();
  });

  it("false if query + results", () => {
    const { component } = render({
      query: "test",
      searchResults: {
        count: 10,
      },
    });
    expect(component).toMatchSnapshot();
  });
});
