/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const expect = require("expect");
const { render } = require("enzyme");

const { createFactory } = require("devtools/client/shared/vendor/react");

const FilterCheckbox = createFactory(
  require("devtools/client/webconsole/components/FilterBar/FilterCheckbox")
);

describe("FilterCheckbox component:", () => {
  const props = {
    label: "test label",
    title: "test title",
    checked: true,
    onChange: () => {},
  };

  it("displays as checked", () => {
    const wrapper = render(FilterCheckbox(props));
    expect(wrapper.is("label")).toBe(true);
    expect(wrapper.attr("title")).toBe("test title");
    expect(wrapper.hasClass("filter-checkbox")).toBe(true);
    expect(wrapper.html()).toBe('<input type="checkbox" checked>test label');
  });

  it("displays as unchecked", () => {
    const wrapper = render(FilterCheckbox({ ...props, checked: false }));
    expect(wrapper.is("label")).toBe(true);
    expect(wrapper.attr("title")).toBe("test title");
    expect(wrapper.hasClass("filter-checkbox")).toBe(true);
    expect(wrapper.html()).toBe('<input type="checkbox">test label');
  });
});
