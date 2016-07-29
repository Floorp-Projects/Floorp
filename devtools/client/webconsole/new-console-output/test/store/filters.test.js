/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const expect = require("expect");

const actions = require("devtools/client/webconsole/new-console-output/actions/filters");
const { getAllMessages } = require("devtools/client/webconsole/new-console-output/selectors/messages");
const { setupStore } = require("devtools/client/webconsole/new-console-output/test/helpers");

describe("Search", () => {
  it("matches on value grips", () => {
    const store = setupStore([
      "console.log('foobar', 'test')",
      "console.warn('danger, will robinson!')",
      "console.log(undefined)"
    ]);
    store.dispatch(actions.filterTextSet("danger"));

    let messages = getAllMessages(store.getState());
    expect(messages.size).toEqual(1);
  });
});
