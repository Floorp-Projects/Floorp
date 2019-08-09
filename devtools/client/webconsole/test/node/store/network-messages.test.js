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
    updatePacket.networkInfo.actor = "message1";
    dispatch(actions.messagesAdd([packet]));
    dispatch(
      actions.networkMessageUpdate(updatePacket.networkInfo, null, updatePacket)
    );
  });

  describe("networkMessagesUpdateById", () => {
    it("adds fetched HTTP request headers", () => {
      const headers = {
        headers: [],
      };

      dispatch(
        actions.networkUpdateRequest("message1", {
          requestHeaders: headers,
        })
      );

      const networkUpdates = getAllNetworkMessagesUpdateById(getState());
      expect(networkUpdates.message1.requestHeaders).toBe(headers);
    });

    it("adds fetched HTTP security info", () => {
      const securityInfo = {
        state: "insecure",
      };

      dispatch(
        actions.networkUpdateRequest("message1", {
          securityInfo: securityInfo,
        })
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
        actions.networkUpdateRequest("message1", {
          requestPostData,
        })
      );

      const { message1 } = getAllNetworkMessagesUpdateById(getState());
      expect(message1.requestPostData).toBe(requestPostData);
      expect(message1.requestHeadersFromUploadStream).toBe(uploadHeaders);
    });
  });
});
