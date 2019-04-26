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

describe("Dummy test", () => {
  it("runs", () => {
    expect(true).toBe(true);
  });
});

/*
const renderer = require("react-test-renderer");
const React = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");

const { MESSAGE_LEVEL } = require("devtools/client/aboutdebugging-new/src/constants");

const Message = React.createFactory(require("devtools/client/aboutdebugging-new/src/components/shared/Message"));

describe("Message component", () => {
  it("renders the expected snapshot for INFO level", () => {
    const message = renderer.create(Message({
      children: dom.div({}, "Message content"),
      className: "some-classname-1",
      level: MESSAGE_LEVEL.INFO,
    }));
    expect(message.toJSON()).toMatchSnapshot();
  });

  it("renders the expected snapshot for WARNING level", () => {
    const message = renderer.create(Message({
      children: dom.div({}, "Message content"),
      className: "some-classname-2",
      level: MESSAGE_LEVEL.WARNING,
    }));
    expect(message.toJSON()).toMatchSnapshot();
  });

  it("renders the expected snapshot for ERROR level", () => {
    const message = renderer.create(Message({
      children: dom.div({}, "Message content"),
      className: "some-classname-3",
      level: MESSAGE_LEVEL.ERROR,
    }));
    expect(message.toJSON()).toMatchSnapshot();
  });
});

describe("Message component renders with closing button", () => {
  it("renders the expected snapshot for Message with closing button", () => {
    const message = renderer.create(Message({
      children: dom.div({}, "Message content"),
      className: "some-classname-1",
      level: MESSAGE_LEVEL.INFO,
      isCloseable: true,
    }));
    expect(message.toJSON()).toMatchSnapshot();
  });
});
*/
