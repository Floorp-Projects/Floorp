/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const { thunk } = require("devtools/client/shared/redux/middleware/thunk");
const configureStore = require("redux-mock-store").default;
const { getRepeatId } = require("devtools/client/webconsole/new-console-output/utils/messages");
const { stubPackets, stubPreparedMessages } = require("devtools/client/webconsole/new-console-output/test/fixtures/stubs/index");
const { setupActions } = require("devtools/client/webconsole/new-console-output/test/helpers");
const constants = require("devtools/client/webconsole/new-console-output/constants");

const mockStore = configureStore([ thunk ]);

const expect = require("expect");

let actions;

describe("Message actions:", () => {
  beforeEach(()=>{
    actions = setupActions();
  });

  describe("messageAdd", () => {
    it("dispatches expected action given a packet", () => {
      const packet = stubPackets.get("console.log('foobar', 'test')");
      const store = mockStore({});
      store.dispatch(actions.messageAdd(packet));

      const expectedActions = store.getActions();
      expect(expectedActions.length).toEqual(1);

      const addAction = expectedActions[0];
      const {message} = addAction;
      const expected = {
        type: constants.MESSAGE_ADD,
        message: stubPreparedMessages.get("console.log('foobar', 'test')")
      };
      expect(message.toJS()).toEqual(expected.message.toJS());
    });

    it("dispatches expected actions given a console.clear packet", () => {
      const packet = stubPackets.get("console.clear()");
      const store = mockStore({});
      store.dispatch(actions.messageAdd(packet));

      const expectedActions = store.getActions();
      expect(expectedActions.length).toEqual(2);

      const [clearAction, addAction] = expectedActions;
      expect(clearAction.type).toEqual(constants.MESSAGES_CLEAR);

      const {message} = addAction;
      const expected = {
        type: constants.MESSAGE_ADD,
        message: stubPreparedMessages.get("console.clear()")
      };
      expect(addAction.type).toEqual(constants.MESSAGE_ADD);
      expect(message.toJS()).toEqual(expected.message.toJS());
    });
  });

  describe("messagesClear", () => {
    it("creates expected action", () => {
      const action = actions.messagesClear();
      const expected = {
        type: constants.MESSAGES_CLEAR,
      };
      expect(action).toEqual(expected);
    });
  });
});
