/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const expect = require("expect");
const { render } = require("enzyme");

const { createFactory } = require("devtools/client/shared/vendor/react");

const FilterButton = createFactory(require("devtools/client/webconsole/new-console-output/components/filter-button").FilterButton);
const { MESSAGE_LEVEL } = require("devtools/client/webconsole/new-console-output/constants");

describe("FilterButton component:", () => {
  const props = {
    active: true,
    label: "Error",
    filterKey: MESSAGE_LEVEL.ERROR,
  };

  it("displays as active when turned on", () => {
    const wrapper = render(FilterButton(props));
    expect(wrapper.html()).toBe(
      "<button class=\"menu-filter-button checked\">Error</button>"
    );
  });

  it("displays as inactive when turned off", () => {
    const inactiveProps = Object.assign({}, props, { active: false });
    const wrapper = render(FilterButton(inactiveProps));
    expect(wrapper.html()).toBe(
      "<button class=\"menu-filter-button\">Error</button>"
    );
  });
});
