/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* eslint-env jest */
const { shallow } = require("enzyme");
const React = require("react");

const WorkerListEmpty = require("devtools/client/application/src/components/WorkerListEmpty");
const WorkerListEmptyComp = React.createFactory(WorkerListEmpty);

/**
 * Dummy test to check if setup is working + we render one component
 */

describe("Dummy test", () => {
  it("shallow renders a component", () => {
    const wrapper = shallow(WorkerListEmptyComp({}));
    expect(wrapper).toMatchSnapshot();
  });
});
