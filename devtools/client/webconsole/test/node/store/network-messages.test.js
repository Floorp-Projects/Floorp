/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const {
  getAllNetworkMessagesUpdateById,
} = require("devtools/client/webconsole/selectors/messages");
const {
  setupActions,
  setupStore,
  clonePacket,
} = require("devtools/client/webconsole/test/node/helpers");
const {
  stubPackets,
} = require("devtools/client/webconsole/test/node/fixtures/stubs/index");

const expect = require("expect");

describe("Network message reducer:", () => {
  let actions;
  let getState;
  let dispatch;

  before(() => {
    actions = setupActions();
  });

  beforeEach(() => {
    const store = setupStore();

    getState = store.getState;
    dispatch = store.dispatch;

    const packet = clonePacket(stubPackets.get("GET request"));
    const updatePacket = clonePacket(stubPackets.get("GET request update"));

    packet.actor = "message1";
    updatePacket.actor = "message1";
    dispatch(actions.messagesAdd([packet]));
    dispatch(actions.networkMessageUpdates([updatePacket], null));
  });

  describe("networkMessagesUpdateById", () => {
    it("adds fetched HTTP request headers", () => {
      const headers = {
        headers: [],
      };

      dispatch(
        actions.networkUpdateRequests([
          {
            id: "message1",
            data: {
              requestHeaders: headers,
            },
          },
        ])
      );

      const networkUpdates = getAllNetworkMessagesUpdateById(getState());
      expect(networkUpdates.message1.requestHeaders).toBe(headers);
    });

    it("makes sure multiple HTTP updates of same request does not override", () => {
      dispatch(
        actions.networkUpdateRequests([
          {
            id: "message1",
            data: {
              stacktrace: [{}],
            },
          },
          {
            id: "message1",
            data: {
              requestHeaders: { headers: [] },
            },
          },
        ])
      );

      const networkUpdates = getAllNetworkMessagesUpdateById(getState());
      expect(networkUpdates.message1.requestHeaders).toNotBe(undefined);
      expect(networkUpdates.message1.stacktrace).toNotBe(undefined);
    });

    it("adds fetched HTTP security info", () => {
      const securityInfo = {
        state: "insecure",
      };

      dispatch(
        actions.networkUpdateRequests([
          {
            id: "message1",
            data: {
              securityInfo,
            },
          },
        ])
      );

      const networkUpdates = getAllNetworkMessagesUpdateById(getState());
      expect(networkUpdates.message1.securityInfo).toBe(securityInfo);
      expect(networkUpdates.message1.securityState).toBe("insecure");
    });

    it("adds fetched HTTP post data", () => {
      const uploadHeaders = Symbol();
      const requestPostData = {
        postData: {
          text: "",
        },
        uploadHeaders,
      };

      dispatch(
        actions.networkUpdateRequests([
          {
            id: "message1",
            data: {
              requestPostData,
            },
          },
        ])
      );

      const { message1 } = getAllNetworkMessagesUpdateById(getState());
      expect(message1.requestPostData).toBe(requestPostData);
      expect(message1.requestHeadersFromUploadStream).toBe(uploadHeaders);
    });
  });
});
