/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React from "devtools/client/shared/vendor/react";
import { shallow } from "enzyme";
import configureStore from "redux-mock-store";

import SearchInput from "../SearchInput";

describe("SearchInput", () => {
  // !! wrapper is defined outside test scope
  // so it will keep values between tests
  const mockStore = configureStore([]);
  const store = mockStore({
    ui: { mutableSearchOptions: { "foo-search": {} } },
  });
  const wrapper = shallow(
    React.createElement(SearchInput, {
      store: store,
      query: "",
      count: 5,
      placeholder: "A placeholder",
      summaryMsg: "So many results",
      showErrorEmoji: false,
      isLoading: false,
      onChange: () => {},
      onKeyDown: () => {},
      searchKey: "foo-search",
      showSearchModifiers: false,
      showExcludePatterns: false,
      showClose: true,
      handleClose: jest.fn(),
      setSearchOptions: jest.fn(),
    })
  ).dive();

  it("renders", () => expect(wrapper).toMatchSnapshot());

  it("shows nav buttons", () => {
    wrapper.setProps({
      handleNext: jest.fn(),
      handlePrev: jest.fn(),
    });
    expect(wrapper).toMatchSnapshot();
  });

  it("shows svg error emoji", () => {
    wrapper.setProps({ showErrorEmoji: true });
    expect(wrapper).toMatchSnapshot();
  });

  it("shows svg magnifying glass", () => {
    wrapper.setProps({ showErrorEmoji: false });
    expect(wrapper).toMatchSnapshot();
  });

  describe("with optional onHistoryScroll", () => {
    const searches = ["foo", "bar", "baz"];
    const createSearch = term => ({
      target: { value: term },
      key: "Enter",
    });

    const scrollUp = currentTerm => ({
      key: "ArrowUp",
      target: { value: currentTerm },
      preventDefault: jest.fn(),
    });
    const scrollDown = currentTerm => ({
      key: "ArrowDown",
      target: { value: currentTerm },
      preventDefault: jest.fn(),
    });

    it("stores entered history in state", () => {
      wrapper.setProps({
        onHistoryScroll: jest.fn(),
        onKeyDown: jest.fn(),
      });
      wrapper.find("input").simulate("keyDown", createSearch(searches[0]));
      expect(wrapper.state().history[0]).toEqual(searches[0]);
    });

    it("stores scroll history in state", () => {
      const onHistoryScroll = jest.fn();
      wrapper.setProps({
        onHistoryScroll,
        onKeyDown: jest.fn(),
      });
      wrapper.find("input").simulate("keyDown", createSearch(searches[0]));
      wrapper.find("input").simulate("keyDown", createSearch(searches[1]));
      expect(wrapper.state().history[0]).toEqual(searches[0]);
      expect(wrapper.state().history[1]).toEqual(searches[1]);
    });

    it("scrolls up stored history on arrow up", () => {
      const onHistoryScroll = jest.fn();
      wrapper.setProps({
        onHistoryScroll,
        onKeyDown: jest.fn(),
      });
      wrapper.find("input").simulate("keyDown", createSearch(searches[0]));
      wrapper.find("input").simulate("keyDown", createSearch(searches[1]));
      wrapper.find("input").simulate("keyDown", scrollUp(searches[1]));
      expect(wrapper.state().history[0]).toEqual(searches[0]);
      expect(wrapper.state().history[1]).toEqual(searches[1]);
      expect(onHistoryScroll).toHaveBeenCalledWith(searches[0]);
    });

    it("scrolls down stored history on arrow down", () => {
      const onHistoryScroll = jest.fn();
      wrapper.setProps({
        onHistoryScroll,
        onKeyDown: jest.fn(),
      });
      wrapper.find("input").simulate("keyDown", createSearch(searches[0]));
      wrapper.find("input").simulate("keyDown", createSearch(searches[1]));
      wrapper.find("input").simulate("keyDown", createSearch(searches[2]));
      wrapper.find("input").simulate("keyDown", scrollUp(searches[2]));
      wrapper.find("input").simulate("keyDown", scrollUp(searches[1]));
      wrapper.find("input").simulate("keyDown", scrollDown(searches[0]));
      expect(onHistoryScroll.mock.calls[2][0]).toBe(searches[1]);
    });
  });
});
