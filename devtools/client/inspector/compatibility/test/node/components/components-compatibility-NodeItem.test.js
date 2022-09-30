/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Unit tests for the NodeItem component.
 */

const { shallow } = require("enzyme");
const React = require("react");

const NodeItem = React.createFactory(
  require("resource://devtools/client/inspector/compatibility/components/NodeItem.js")
);

describe("NodeItem component", () => {
  it("renders a node", () => {
    const pane = shallow(
      NodeItem({
        node: {
          actorID: "test-actor",
          attributes: [],
          nodeName: "test-element",
          nodeType: 1,
        },
      })
    );
    expect(pane).toMatchSnapshot();
  });
});
