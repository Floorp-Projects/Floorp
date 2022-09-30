/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Unit tests for the shared/Message component.
 */

const { shallow } = require("enzyme");
const React = require("react");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");

const {
  MESSAGE_LEVEL,
} = require("resource://devtools/client/aboutdebugging/src/constants.js");

const Message = React.createFactory(
  require("resource://devtools/client/aboutdebugging/src/components/shared/Message.js")
);

describe("Message component", () => {
  it("renders the expected snapshot for INFO level", () => {
    const message = shallow(
      Message({
        children: dom.div({}, "Message content"),
        className: "some-classname-1",
        level: MESSAGE_LEVEL.INFO,
      })
    );
    expect(message).toMatchSnapshot();
  });

  it("renders the expected snapshot for WARNING level", () => {
    const message = shallow(
      Message({
        children: dom.div({}, "Message content"),
        className: "some-classname-2",
        level: MESSAGE_LEVEL.WARNING,
      })
    );
    expect(message).toMatchSnapshot();
  });

  it("renders the expected snapshot for ERROR level", () => {
    const message = shallow(
      Message({
        children: dom.div({}, "Message content"),
        className: "some-classname-3",
        level: MESSAGE_LEVEL.ERROR,
      })
    );
    expect(message).toMatchSnapshot();
  });
});

describe("Message component renders with closing button", () => {
  it("renders the expected snapshot for Message with closing button", () => {
    const message = shallow(
      Message({
        children: dom.div({}, "Message content"),
        className: "some-classname-1",
        level: MESSAGE_LEVEL.INFO,
        isCloseable: true,
      })
    );
    expect(message).toMatchSnapshot();
  });
});
