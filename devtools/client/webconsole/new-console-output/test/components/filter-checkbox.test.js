/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const expect = require("expect");
const { render } = require("enzyme");

const { createFactory } = require("devtools/client/shared/vendor/react");

const FilterCheckbox = createFactory(require("devtools/client/webconsole/new-console-output/components/filter-checkbox"));

describe("FilterCheckbox component:", () => {
  const props = {
    label: "test label",
    title: "test title",
    checked: true,
    onChange: () => {},
  };

  it("displays as checked", () => {
    const wrapper = render(FilterCheckbox(props));
    expect(wrapper.html()).toBe(
      '<label title="test title" class="filter-checkbox">' +
      '<input type="checkbox" checked>test label</label>'
    );
  });

  it("displays as unchecked", () => {
    const uncheckedProps = Object.assign({}, props, { checked: false });
    const wrapper = render(FilterCheckbox(uncheckedProps));
    expect(wrapper.html()).toBe(
      '<label title="test title" class="filter-checkbox">' +
      '<input type="checkbox">test label</label>'
    );
  });
});
