/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const expect = require("expect");

const actions = require("devtools/client/webconsole/new-console-output/actions/messages");
const { getAllMessages } = require("devtools/client/webconsole/new-console-output/selectors/messages");
const { configureStore } = require("devtools/client/webconsole/new-console-output/store");
const { stubConsoleMessages } = require("devtools/client/webconsole/new-console-output/test/fixtures/stubs");
const Services = require("devtools/client/webconsole/new-console-output/test/fixtures/Services");

describe("Search", () => {
  it("matches on value grips", () => {
    const store = setupStore([
      "console.log('foobar', 'test')",
      "console.warn('danger, will robinson!')",
      "console.log(undefined)"
    ]);
    store.dispatch(actions.messagesSearch("danger"));

    let messages = getAllMessages(store.getState());
    expect(messages.size).toEqual(1);
  });
});

function setupStore(input) {
  const store = configureStore(Services);
  addMessages(input, store.dispatch);
  return store;
}

function addMessages(input, dispatch) {
  input.forEach((cmd) => {
    dispatch(actions.messageAdd(stubConsoleMessages.get(cmd)));
  });
}

