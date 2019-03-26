/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Unit tests for the shared/Message component.
 */

const renderer = require("react-test-renderer");
const React = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");

const { MESSAGE_LEVEL } = require("devtools/client/aboutdebugging-new/src/constants");

const Message = React.createFactory(require("devtools/client/aboutdebugging-new/src/components/shared/Message"));

describe("Message component", () => {
  it("renders the expected snapshot for INFO level", () => {
    const message = renderer.create(Message({
      children: dom.div({}, "Message content"),
      className: "some-classname",
      level: MESSAGE_LEVEL.INFO,
    }));
    expect(message.toJSON()).toMatchSnapshot();
  });
});
