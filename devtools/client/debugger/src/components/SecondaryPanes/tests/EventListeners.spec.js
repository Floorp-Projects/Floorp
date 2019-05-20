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
});
