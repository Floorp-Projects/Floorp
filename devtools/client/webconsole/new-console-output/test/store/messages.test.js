/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const { getAllMessages } = require("devtools/client/webconsole/new-console-output/selectors/messages");
const { getRepeatId } = require("devtools/client/webconsole/new-console-output/utils/messages");
const {
  setupActions,
  setupStore
} = require("devtools/client/webconsole/new-console-output/test/helpers");
const { stubConsoleMessages } = require("devtools/client/webconsole/new-console-output/test/fixtures/stubs");

const expect = require("expect");

describe("Message reducer:", () => {
  let actions;

  before(() => {
    actions = setupActions();
  });

  it("adds a message to an empty store", () => {
    const { dispatch, getState } = setupStore([]);

    const message = stubConsoleMessages.get("console.log('foobar', 'test')");
    dispatch(actions.messageAdd(message));

    const messages = getAllMessages(getState());

    // @TODO Remove repeatId once stubs are generated using prepareMessage.
    let expected = message.set("repeatId", getRepeatId(message)).set("id", "1");
    expect(messages.first()).toEqual(expected);
  });

  it("increments repeat on a repeating message", () => {
    const { dispatch, getState } = setupStore([
      "console.log('foobar', 'test')",
      "console.log('foobar', 'test')"
    ]);

    const message = stubConsoleMessages.get("console.log('foobar', 'test')");
    dispatch(actions.messageAdd(message));
    dispatch(actions.messageAdd(message));

    const messages = getAllMessages(getState());

    expect(messages.size).toBe(1);
    expect(messages.first().repeat).toBe(4);
  });

  it("does not clobber a unique message", () => {
    const { dispatch, getState } = setupStore([
      "console.log('foobar', 'test')",
      "console.log('foobar', 'test')"
    ]);

    const message = stubConsoleMessages.get("console.log('foobar', 'test')");
    dispatch(actions.messageAdd(message));

    const message2 = stubConsoleMessages.get("console.log(undefined)");
    dispatch(actions.messageAdd(message2));

    const messages = getAllMessages(getState());

    expect(messages.size).toBe(2);
    expect(messages.first().repeat).toBe(3);
    expect(messages.last().repeat).toBe(1);
  });

  it("clears the store in response to console.clear()", () => {
    const { dispatch, getState } = setupStore([
      "console.log('foobar', 'test')",
      "console.log(undefined)"
    ]);

    dispatch(actions.messageAdd(stubConsoleMessages.get("console.clear()")));

    const messages = getAllMessages(getState());

    expect(messages.size).toBe(1);
    expect(messages.first().parameters[0]).toBe("Console cleared.");
  });

  it("limits the number of messages displayed", () => {
    const { dispatch, getState } = setupStore([]);

    const logLimit = 1000;
    const baseMessage = stubConsoleMessages.get("console.log(undefined)");
    for (let i = 1; i <= logLimit + 1; i++) {
      const msg = baseMessage.set("parameters", [`message num ${i}`]);
      dispatch(actions.messageAdd(msg));
    }

    const messages = getAllMessages(getState());
    expect(messages.count()).toBe(logLimit);
    expect(messages.first().parameters[0]).toBe(`message num 2`);
    expect(messages.last().parameters[0]).toBe(`message num ${logLimit + 1}`);
  });
});
