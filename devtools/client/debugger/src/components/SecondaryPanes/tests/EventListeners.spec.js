/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import React from "react";
import { shallow } from "enzyme";
import EventListeners from "../EventListeners";

function getCategories() {
  return [
    {
      name: "Category 1",
      events: [
        { name: "Subcategory 1", id: "category1.subcategory1" },
        { name: "Subcategory 2", id: "category1.subcategory2" },
      ],
    },
    {
      name: "Category 2",
      events: [
        { name: "Subcategory 3", id: "category2.subcategory1" },
        { name: "Subcategory 4", id: "category2.subcategory2" },
      ],
    },
  ];
}

function generateDefaults(overrides = {}) {
  const defaults = {
    activeEventListeners: [],
    expandedCategories: [],
    categories: [],
  };

  return { ...defaults, ...overrides };
}

function render(overrides = {}) {
  const props = generateDefaults(overrides);
  // $FlowIgnore
  const component = shallow(<EventListeners.WrappedComponent {...props} />);
  return { component, props };
}

describe("EventListeners", () => {
  it("should render", async () => {
    const { component } = render();
    expect(component).toMatchSnapshot();
  });

  it("should render categories appropriately", async () => {
    const props = {
      ...generateDefaults(),
      categories: getCategories(),
    };
    const { component } = render(props);
    expect(component).toMatchSnapshot();
  });

  it("should render expanded categories appropriately", async () => {
    const props = {
      ...generateDefaults(),
      categories: getCategories(),
      expandedCategories: ["Category 2"],
    };
    const { component } = render(props);
    expect(component).toMatchSnapshot();
  });

  it("should render checked subcategories appropriately", async () => {
    const props = {
      ...generateDefaults(),
      categories: getCategories(),
      activeEventListeners: ["category1.subcategory2"],
      expandedCategories: ["Category 1"],
    };
    const { component } = render(props);
    expect(component).toMatchSnapshot();
  });

  it("should filter the event listeners based on the event name", async () => {
    const props = {
      ...generateDefaults(),
      categories: getCategories(),
    };
    const { component } = render(props);
    component.find(".event-search-input").simulate("focus");

    const searchInput = component.find(".event-search-input");
    // Simulate a search query of "Subcategory 3" to display just one event which
    // will be the Subcategory 3 event
    searchInput.simulate("change", { target: { value: "Subcategory 3" } });

    const displayedEvents = component.find(".event-listener-event");
    expect(displayedEvents).toHaveLength(1);
  });

  it("should filter the event listeners based on the category name", async () => {
    const props = {
      ...generateDefaults(),
      categories: getCategories(),
    };
    const { component } = render(props);
    component.find(".event-search-input").simulate("focus");

    const searchInput = component.find(".event-search-input");
    // Simulate a search query of "Category 1" to display two events which will be
    // the Subcategory 1 event and the Subcategory 2 event
    searchInput.simulate("change", { target: { value: "Category 1" } });

    const displayedEvents = component.find(".event-listener-event");
    expect(displayedEvents).toHaveLength(2);
  });

  it("should be case insensitive when filtering events and categories", async () => {
    const props = {
      ...generateDefaults(),
      categories: getCategories(),
    };
    const { component } = render(props);
    component.find(".event-search-input").simulate("focus");

    const searchInput = component.find(".event-search-input");
    // Simulate a search query of "Subcategory 3" to display just one event which
    // will be the Subcategory 3 event
    searchInput.simulate("change", { target: { value: "sUbCaTeGoRy 3" } });

    const displayedEvents = component.find(".event-listener-event");
    expect(displayedEvents).toHaveLength(1);
  });
});
