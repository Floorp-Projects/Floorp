/* eslint max-nested-callbacks: ["error", 4] */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React from "react";
import { Provider } from "react-redux";
import configureStore from "redux-mock-store";

import { shallow, mount } from "enzyme";
import { QuickOpenModal } from "../QuickOpenModal";
import { getDisplayURL } from "../../utils/sources-tree/getURL";
import { searchKeys } from "../../constants";

jest.mock("fuzzaldrin-plus", () => {
  return {
    filter: jest.fn(() => []),
    prepareQuery: jest.fn(() => {}),
    wrap: jest.fn(() => {}),
  };
});

import { filter } from "fuzzaldrin-plus";

function generateModal(propOverrides, renderType = "shallow") {
  const mockStore = configureStore([]);
  const store = mockStore({
    ui: {
      mutableSearchOptions: {
        [searchKeys.QUICKOPEN_SEARCH]: {
          regexMatch: false,
          wholeWord: false,
          caseSensitive: false,
          excludePatterns: "",
        },
      },
    },
  });
  const props = {
    enabled: false,
    query: "",
    searchType: "sources",
    displayedSources: [],
    blackBoxRanges: {},
    openedTabUrls: [],
    selectedLocation: { source: { id: "foo" } },
    selectSpecificLocation: jest.fn(),
    setQuickOpenQuery: jest.fn(),
    highlightLineRange: jest.fn(),
    clearHighlightLineRange: jest.fn(),
    closeQuickOpen: jest.fn(),
    getFunctionSymbols: jest.fn(() => []),
    shortcutsModalEnabled: false,
    toggleShortcutsModal: jest.fn(),
    isOriginal: false,
    thread: "FakeThread",
    ...propOverrides,
  };
  return {
    wrapper:
      renderType === "shallow"
        ? shallow(
            <Provider store={store}>
              <QuickOpenModal {...props} />
            </Provider>
          ).dive()
        : mount(
            <Provider store={store}>
              <QuickOpenModal {...props} />
            </Provider>
          ),
    props,
  };
}

async function waitForUpdateResultsThrottle() {
  await new Promise(res =>
    setTimeout(res, QuickOpenModal.UPDATE_RESULTS_THROTTLE)
  );
}

describe("QuickOpenModal", () => {
  beforeEach(() => {
    filter.mockClear();
  });
  test("Doesn't render when disabled", () => {
    const { wrapper } = generateModal();
    expect(wrapper).toMatchSnapshot();
  });

  test("Renders when enabled", () => {
    const { wrapper } = generateModal({ enabled: true });
    expect(wrapper).toMatchSnapshot();
  });

  test("Basic render with mount", () => {
    const { wrapper } = generateModal({ enabled: true }, "mount");
    expect(wrapper).toMatchSnapshot();
  });

  test("Basic render with mount & searchType = functions", () => {
    const { wrapper } = generateModal(
      {
        enabled: true,
        query: "@",
        searchType: "functions",
      },
      "mount"
    );
    expect(wrapper).toMatchSnapshot();
  });

  test("toggles shortcut modal if enabled", () => {
    const { props } = generateModal(
      {
        enabled: true,
        query: "test",
        shortcutsModalEnabled: true,
        toggleShortcutsModal: jest.fn(),
      },
      "shallow"
    );
    expect(props.toggleShortcutsModal).toHaveBeenCalled();
  });

  test("shows top sources", () => {
    const { wrapper } = generateModal(
      {
        enabled: true,
        query: "",
        displayedSources: [
          {
            url: "mozilla.com",
            displayURL: getDisplayURL("mozilla.com"),
          },
        ],
        openedTabUrls: ["mozilla.com"],
      },
      "shallow"
    );
    expect(wrapper.state("results")).toEqual([
      {
        id: undefined,
        icon: "tab result-item-icon",
        subtitle: "mozilla.com",
        title: "mozilla.com",
        url: "mozilla.com",
        value: "mozilla.com",
        source: {
          url: "mozilla.com",
          displayURL: getDisplayURL("mozilla.com"),
        },
      },
    ]);
  });

  describe("shows loading", () => {
    it("loads with function type search", () => {
      const { wrapper } = generateModal(
        {
          enabled: true,
          query: "",
          searchType: "functions",
        },
        "shallow"
      );
      expect(wrapper).toMatchSnapshot();
    });
  });

  test("Basic render with mount & searchType = variables", () => {
    const { wrapper } = generateModal(
      {
        enabled: true,
        query: "#",
        searchType: "variables",
      },
      "mount"
    );
    expect(wrapper).toMatchSnapshot();
  });

  test("Basic render with mount & searchType = shortcuts", () => {
    const { wrapper } = generateModal(
      {
        enabled: true,
        query: "?",
        searchType: "shortcuts",
      },
      "mount"
    );
    expect(wrapper.find("ResultList")).toHaveLength(1);
    expect(wrapper.find("li")).toHaveLength(3);
  });

  test("updateResults on enable", () => {
    const { wrapper } = generateModal({}, "mount");
    expect(wrapper).toMatchSnapshot();
    wrapper.setProps({ enabled: true });
    expect(wrapper).toMatchSnapshot();
  });

  test("basic source search", async () => {
    const { wrapper } = generateModal(
      {
        enabled: true,
      },
      "mount"
    );
    wrapper.find("input").simulate("change", { target: { value: "somefil" } });
    await waitForUpdateResultsThrottle();
    expect(filter).toHaveBeenCalledWith([], "somefil", {
      key: "value",
      maxResults: 100,
    });
  });

  test("basic gotoSource search", async () => {
    const { wrapper } = generateModal(
      {
        enabled: true,
        searchType: "gotoSource",
      },
      "mount"
    );
    wrapper
      .find("input")
      .simulate("change", { target: { value: "somefil:33" } });

    await waitForUpdateResultsThrottle();

    expect(filter).toHaveBeenCalledWith([], "somefil", {
      key: "value",
      maxResults: 100,
    });
  });

  describe("empty symbol search", () => {
    it("basic symbol search", async () => {
      const { wrapper } = generateModal(
        {
          enabled: true,
          searchType: "functions",
          // symbol searching relies on a source being selected.
          // So we dummy out the source and the API.
          selectedLocation: { source: { id: "foo", text: "yo" } },
          selectedContentLoaded: true,
        },
        "mount"
      );

      wrapper
        .find("input")
        .simulate("change", { target: { value: "@someFunc" } });
      await waitForUpdateResultsThrottle();
      expect(filter).toHaveBeenCalledWith([], "someFunc", {
        key: "name",
        maxResults: 100,
        preparedQuery: undefined,
      });
    });

    it("does not do symbol search if no selected source", () => {
      const { wrapper } = generateModal(
        {
          enabled: true,
          searchType: "functions",

          // symbol searching relies on a source being selected.
          // So we dummy out the source and the API.
          selectedLocation: null,
          selectedContentLoaded: false,
        },
        "mount"
      );
      wrapper
        .find("input")
        .simulate("change", { target: { value: "@someFunc" } });
      expect(filter).not.toHaveBeenCalled();
    });
  });

  test("Simple goto search query = :abc & searchType = goto", () => {
    const { wrapper } = generateModal(
      {
        enabled: true,
        query: ":abc",
        searchType: "goto",
      },
      "mount"
    );
    expect(wrapper.childAt(0)).toMatchSnapshot();
    expect(wrapper.childAt(0).state().results).toEqual(null);
  });

  describe("onEnter", () => {
    it("on Enter go to location", () => {
      const { wrapper, props } = generateModal(
        {
          enabled: true,
          query: ":34:12",
          searchType: "goto",
          selectedLocation: { source: { id: "foo" } },
        },
        "shallow"
      );
      const event = {
        key: "Enter",
      };
      wrapper.find("Connect(SearchInput)").simulate("keydown", event);
      expect(props.selectSpecificLocation).toHaveBeenCalledWith({
        column: 11,
        line: 34,
        source: {
          id: "foo",
        },
        sourceActorId: undefined,
        sourceActor: null,
      });
    });

    it("on Enter go to location with sourceId", () => {
      const sourceId = "source_id";
      const { wrapper, props } = generateModal(
        {
          enabled: true,
          query: ":34:12",
          searchType: "goto",
          selectedLocation: { source: { id: sourceId } },
          selectedContentLoaded: true,
        },
        "shallow"
      );
      const event = {
        key: "Enter",
      };
      wrapper.find("Connect(SearchInput)").simulate("keydown", event);
      expect(props.selectSpecificLocation).toHaveBeenCalledWith({
        column: 11,
        line: 34,
        source: {
          id: sourceId,
        },
        sourceActorId: undefined,
        sourceActor: null,
      });
    });

    it("on Enter with no location, does no action", () => {
      const { wrapper, props } = generateModal(
        {
          enabled: true,
          query: ":",
          searchType: "goto",
        },
        "shallow"
      );
      const event = {
        key: "Enter",
      };
      wrapper.find("Connect(SearchInput)").simulate("keydown", event);
      expect(props.setQuickOpenQuery).not.toHaveBeenCalled();
      expect(props.selectSpecificLocation).not.toHaveBeenCalled();
      expect(props.highlightLineRange).not.toHaveBeenCalled();
    });

    it("on Enter with empty results, handle no item", () => {
      const { wrapper, props } = generateModal(
        {
          enabled: true,
          query: "",
          searchType: "shortcuts",
        },
        "shallow"
      );
      wrapper.setState(() => ({
        results: [],
        selectedIndex: 0,
      }));
      const event = {
        key: "Enter",
      };
      wrapper.find("Connect(SearchInput)").simulate("keydown", event);
      expect(props.setQuickOpenQuery).not.toHaveBeenCalled();
      expect(props.selectSpecificLocation).not.toHaveBeenCalled();
      expect(props.highlightLineRange).not.toHaveBeenCalled();
    });

    it("on Enter with results, handle symbol shortcut", () => {
      const symbols = [":", "#", "@"];
      for (const symbol of symbols) {
        const { wrapper, props } = generateModal(
          {
            enabled: true,
            query: "",
            searchType: "shortcuts",
          },
          "shallow"
        );
        wrapper.setState(() => ({
          results: [{ id: symbol }],
          selectedIndex: 0,
        }));
        const event = {
          key: "Enter",
        };
        wrapper.find("Connect(SearchInput)").simulate("keydown", event);
        expect(props.setQuickOpenQuery).toHaveBeenCalledWith(symbol);
      }
    });

    it("on Enter, returns the result with the selected index", () => {
      const { wrapper, props } = generateModal(
        {
          enabled: true,
          query: "@test",
          searchType: "shortcuts",
        },
        "shallow"
      );
      wrapper.setState(() => ({
        results: [{ id: "@" }, { id: ":" }, { id: "#" }],
        selectedIndex: 1,
      }));
      const event = {
        key: "Enter",
      };
      wrapper.find("Connect(SearchInput)").simulate("keydown", event);
      expect(props.setQuickOpenQuery).toHaveBeenCalledWith(":");
    });

    it("on Enter with results, handle result item", () => {
      const id = "test_id";
      const { wrapper, props } = generateModal(
        {
          enabled: true,
          query: "@test",
          searchType: "other",
          selectedLocation: { source: { id } },
        },
        "shallow"
      );
      wrapper.setState(() => ({
        results: [{}, { id }],
        selectedIndex: 1,
      }));
      const event = {
        key: "Enter",
      };
      wrapper.find("Connect(SearchInput)").simulate("keydown", event);
      expect(props.selectSpecificLocation).toHaveBeenCalledWith({
        column: undefined,
        line: 0,
        source: { id },
        sourceActorId: undefined,
        sourceActor: null,
      });
      expect(props.setQuickOpenQuery).not.toHaveBeenCalled();
    });

    it("on Enter with results, handle functions result item", () => {
      const id = "test_id";
      const { wrapper, props } = generateModal(
        {
          enabled: true,
          query: "@test",
          searchType: "functions",
          selectedLocation: { source: { id } },
        },
        "shallow"
      );
      wrapper.setState(() => ({
        results: [{}, { id }],
        selectedIndex: 1,
      }));
      const event = {
        key: "Enter",
      };
      wrapper.find("Connect(SearchInput)").simulate("keydown", event);
      expect(props.selectSpecificLocation).toHaveBeenCalledWith({
        column: undefined,
        line: 0,
        source: { id },
        sourceActorId: undefined,
        sourceActor: null,
      });
      expect(props.setQuickOpenQuery).not.toHaveBeenCalled();
    });

    it("on Enter with results, handle gotoSource search", () => {
      const id = "test_id";
      const { wrapper, props } = generateModal(
        {
          enabled: true,
          query: ":3:4",
          searchType: "gotoSource",
          selectedLocation: { source: { id } },
        },
        "shallow"
      );
      wrapper.setState(() => ({
        results: [{}, { id }],
        selectedIndex: 1,
      }));
      const event = {
        key: "Enter",
      };
      wrapper.find("Connect(SearchInput)").simulate("keydown", event);
      expect(props.selectSpecificLocation).toHaveBeenCalledWith({
        column: 3,
        line: 3,
        source: { id },
        sourceActorId: undefined,
        sourceActor: null,
      });
      expect(props.setQuickOpenQuery).not.toHaveBeenCalled();
    });

    it("on Enter with results, handle shortcuts search", () => {
      const { wrapper, props } = generateModal(
        {
          enabled: true,
          query: "@",
          searchType: "shortcuts",
        },
        "shallow"
      );
      const id = "#";
      wrapper.setState(() => ({
        results: [{}, { id }],
        selectedIndex: 1,
      }));
      const event = {
        key: "Enter",
      };
      wrapper.find("Connect(SearchInput)").simulate("keydown", event);
      expect(props.selectSpecificLocation).not.toHaveBeenCalled();
      expect(props.setQuickOpenQuery).toHaveBeenCalledWith(id);
    });
  });

  describe("onKeyDown", () => {
    it("does nothing if search type is not goto", () => {
      const { wrapper, props } = generateModal(
        {
          enabled: true,
          query: "test",
          searchType: "other",
        },
        "shallow"
      );
      wrapper.find("Connect(SearchInput)").simulate("keydown", {});
      expect(props.selectSpecificLocation).not.toHaveBeenCalled();
      expect(props.setQuickOpenQuery).not.toHaveBeenCalled();
    });

    it("on Tab, close modal", () => {
      const { wrapper, props } = generateModal(
        {
          enabled: true,
          query: ":34:12",
          searchType: "goto",
        },
        "shallow"
      );
      const event = {
        key: "Tab",
      };
      wrapper.find("Connect(SearchInput)").simulate("keydown", event);
      expect(props.closeQuickOpen).toHaveBeenCalled();
      expect(props.selectSpecificLocation).not.toHaveBeenCalled();
    });
  });

  describe("with arrow keys", () => {
    it("on ArrowUp, traverse results up with functions", () => {
      const sourceId = "sourceId";
      const { wrapper, props } = generateModal(
        {
          enabled: true,
          query: "test",
          searchType: "functions",
          selectedLocation: { source: { id: sourceId } },
          selectedContentLoaded: true,
        },
        "shallow"
      );
      const event = {
        preventDefault: jest.fn(),
        key: "ArrowUp",
      };
      const location = {
        sourceId: "sourceId",
        start: {
          line: 1,
        },
        end: {
          line: 3,
        },
      };

      wrapper.setState(() => ({
        results: [{ id: "0", location }, { id: "1" }, { id: "2" }],
        selectedIndex: 1,
      }));
      wrapper.find("Connect(SearchInput)").simulate("keydown", event);
      expect(event.preventDefault).toHaveBeenCalled();
      expect(wrapper.state().selectedIndex).toEqual(0);
      expect(props.highlightLineRange).toHaveBeenCalledWith({
        sourceId: "sourceId",
        end: 3,
        start: 1,
      });
    });

    it("on ArrowDown, traverse down with no results", () => {
      const { wrapper, props } = generateModal(
        {
          enabled: true,
          query: "test",
          searchType: "goto",
        },
        "shallow"
      );
      const event = {
        preventDefault: jest.fn(),
        key: "ArrowDown",
      };
      wrapper.setState(() => ({
        results: null,
        selectedIndex: 1,
      }));
      wrapper.find("Connect(SearchInput)").simulate("keydown", event);
      expect(event.preventDefault).toHaveBeenCalled();
      expect(wrapper.state().selectedIndex).toEqual(0);
      expect(props.selectSpecificLocation).not.toHaveBeenCalledWith();
      expect(props.highlightLineRange).not.toHaveBeenCalled();
    });

    it("on ArrowUp, traverse results up to function with no location", () => {
      const sourceId = "sourceId";
      const { wrapper, props } = generateModal(
        {
          enabled: true,
          query: "test",
          searchType: "functions",
          selectedLocation: { source: { id: sourceId } },
          selectedContentLoaded: true,
        },
        "shallow"
      );
      const event = {
        preventDefault: jest.fn(),
        key: "ArrowUp",
      };
      wrapper.setState(() => ({
        results: [{ id: "0", location: null }, { id: "1" }, { id: "2" }],
        selectedIndex: 1,
      }));
      wrapper.find("Connect(SearchInput)").simulate("keydown", event);
      expect(event.preventDefault).toHaveBeenCalled();
      expect(wrapper.state().selectedIndex).toEqual(0);
      expect(props.highlightLineRange).not.toHaveBeenCalled();
      expect(props.clearHighlightLineRange).toHaveBeenCalled();
    });

    it(
      "on ArrowDown, traverse down results, without " +
        "taking action if no selectedSource",
      () => {
        const { wrapper, props } = generateModal(
          {
            enabled: true,
            query: "test",
            searchType: "variables",
            selectedLocation: null,
            selectedContentLoaded: true,
          },
          "shallow"
        );
        const event = {
          preventDefault: jest.fn(),
          key: "ArrowDown",
        };
        const location = {
          sourceId: "sourceId",
          start: {
            line: 7,
          },
        };
        wrapper.setState(() => ({
          results: [{ id: "0", location }, { id: "1" }, { id: "2" }],
          selectedIndex: 1,
        }));
        wrapper.find("Connect(SearchInput)").simulate("keydown", event);
        expect(event.preventDefault).toHaveBeenCalled();
        expect(wrapper.state().selectedIndex).toEqual(2);
        expect(props.selectSpecificLocation).not.toHaveBeenCalled();
        expect(props.highlightLineRange).not.toHaveBeenCalled();
      }
    );

    it(
      "on ArrowUp, traverse up results, without taking action if " +
        "the query is not for variables or functions",
      () => {
        const sourceId = "sourceId";
        const { wrapper, props } = generateModal(
          {
            enabled: true,
            query: "test",
            searchType: "other",
            selectedLocation: { source: { id: sourceId } },
            selectedContentLoaded: true,
          },
          "shallow"
        );
        const event = {
          preventDefault: jest.fn(),
          key: "ArrowUp",
        };
        const location = {
          sourceId: "sourceId",
          start: {
            line: 7,
          },
        };
        wrapper.setState(() => ({
          results: [{ id: "0", location }, { id: "1" }, { id: "2" }],
          selectedIndex: 1,
        }));
        wrapper.find("Connect(SearchInput)").simulate("keydown", event);
        expect(event.preventDefault).toHaveBeenCalled();
        expect(wrapper.state().selectedIndex).toEqual(0);
        expect(props.selectSpecificLocation).not.toHaveBeenCalled();
        expect(props.highlightLineRange).not.toHaveBeenCalled();
      }
    );
  });

  describe("showErrorEmoji", () => {
    it("true when no count + query", () => {
      const { wrapper } = generateModal(
        {
          enabled: true,
          query: "test",
          searchType: "other",
        },
        "mount"
      );
      expect(wrapper).toMatchSnapshot();
    });

    it("false when count + query", () => {
      const { wrapper } = generateModal(
        {
          enabled: true,
          query: "dasdasdas",
        },
        "mount"
      );
      wrapper.setState(() => ({
        results: [1, 2],
      }));
      expect(wrapper).toMatchSnapshot();
    });

    it("false when no query", () => {
      const { wrapper } = generateModal(
        {
          enabled: true,
          query: "",
          searchType: "other",
        },
        "mount"
      );
      expect(wrapper).toMatchSnapshot();
    });

    it("false when goto numeric ':2222'", () => {
      const { wrapper } = generateModal(
        {
          enabled: true,
          query: ":2222",
          searchType: "goto",
        },
        "mount"
      );
      expect(wrapper).toMatchSnapshot();
    });

    it("true when goto not numeric ':22k22'", () => {
      const { wrapper } = generateModal(
        {
          enabled: true,
          query: ":22k22",
          searchType: "goto",
        },
        "mount"
      );
      expect(wrapper).toMatchSnapshot();
    });
  });
});
