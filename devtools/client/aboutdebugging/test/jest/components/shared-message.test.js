/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Unit tests for the shared/Message component.
 */

/**
 * ============================================================
 * PLEASE NOTE:
 * This test is failing due to https://bugzilla.mozilla.org/show_bug.cgi?id=1546370
 * The fix described in 1546370 is depending on try supporting a Node version >=10
 * ============================================================
 */

const { shallow } = require("enzyme");
const React = require("react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");

const {
  MESSAGE_LEVEL,
} = require("devtools/client/aboutdebugging/src/constants");

const Message = React.createFactory(
  require("devtools/client/aboutdebugging/src/components/shared/Message")
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
