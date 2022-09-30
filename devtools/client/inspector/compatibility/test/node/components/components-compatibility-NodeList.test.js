/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Unit tests for the NodeList component.
 */

const { shallow } = require("enzyme");
const React = require("react");

const NodeList = React.createFactory(
  require("resource://devtools/client/inspector/compatibility/components/NodeList.js")
);

describe("NodeList component", () => {
  it("renders a node", () => {
    const pane = shallow(
      NodeList({
        nodes: [
          {
            actorID: "test-actor",
            attributes: [],
            nodeName: "test-element",
            nodeType: 1,
          },
        ],
      })
    );
    expect(pane).toMatchSnapshot();
  });
});

describe("NodeList component", () => {
  it("renders some nodes", () => {
    const pane = shallow(
      NodeList({
        nodes: [
          {
            actorID: "test-actor-1",
            attributes: [],
            nodeName: "test-element-1",
            nodeType: 1,
          },
          {
            actorID: "test-actor-2",
            attributes: [],
            nodeName: "test-element-2",
            nodeType: 1,
          },
        ],
      })
    );
    expect(pane).toMatchSnapshot();
  });
});
