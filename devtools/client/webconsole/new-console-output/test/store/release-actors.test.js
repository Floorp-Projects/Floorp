/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const {
  setupActions,
  setupStore,
  clonePacket
} = require("devtools/client/webconsole/new-console-output/test/helpers");
const { stubPackets } = require("devtools/client/webconsole/new-console-output/test/fixtures/stubs/index");

const expect = require("expect");

describe("Release actor enhancer:", () => {
  let actions;

  before(() => {
    actions = setupActions();
  });

  describe("Client proxy", () => {
    it("properly releases backend actors when limit is reached", () => {
      let proxyExecuted = 0;
      const { dispatch } = setupStore([], {
        proxy: {
          releaseActor: (actor) => {
            proxyExecuted++;
          }
        }
      });

      const logCount = 1001;
      const packet = clonePacket(stubPackets.get(
        "console.assert(false, {message: 'foobar'})"));

      for (let i = 1; i <= logCount; i++) {
        packet.message.arguments.push(`message num ${i}`);
        dispatch(actions.messageAdd(packet));
      }

      expect(proxyExecuted).toBe(1);
    });

    it("properly releases backend actors after clear", () => {
      let proxyExecuted = 0;
      const { dispatch } = setupStore([], {
        proxy: {
          releaseActor: (actor) => {
            proxyExecuted++;
          }
        }
      });

      dispatch(actions.messageAdd(clonePacket(stubPackets.get(
        "console.assert(false, {message: 'foobar'})"))));
      dispatch(actions.messagesClear());

      expect(proxyExecuted).toBe(1);
    });
  });
});
