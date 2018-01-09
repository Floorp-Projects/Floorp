/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const {
  getAllNetworkMessagesUpdateById,
} = require("devtools/client/webconsole/new-console-output/selectors/messages");
const {
  setupActions,
  setupStore,
  clonePacket
} = require("devtools/client/webconsole/new-console-output/test/helpers");
const { stubPackets } = require("devtools/client/webconsole/new-console-output/test/fixtures/stubs/index");

const expect = require("expect");

describe("Network message reducer:", () => {
  let actions;
  let getState;
  let dispatch;

  before(() => {
    actions = setupActions();
  });

  beforeEach(() => {
    const store = setupStore([]);

    getState = store.getState;
    dispatch = store.dispatch;

    let packet = clonePacket(stubPackets.get("GET request"));
    let updatePacket = clonePacket(stubPackets.get("GET request update"));

    packet.actor = "message1";
    updatePacket.networkInfo.actor = "message1";
    dispatch(actions.messagesAdd([packet]));
    dispatch(actions.networkMessageUpdate(updatePacket.networkInfo, null, updatePacket));
  });

  describe("networkMessagesUpdateById", () => {
    it("adds fetched HTTP request headers", () => {
      let headers = {
        headers: []
      };

      dispatch(actions.networkUpdateRequest("message1", {
        requestHeaders: headers
      }));

      let networkUpdates = getAllNetworkMessagesUpdateById(getState());
      expect(networkUpdates.message1.requestHeaders).toBe(headers);
    });

    it("adds fetched HTTP security info", () => {
      let securityInfo = {
        state: "insecure"
      };

      dispatch(actions.networkUpdateRequest("message1", {
        securityInfo: securityInfo
      }));

      let networkUpdates = getAllNetworkMessagesUpdateById(getState());
      expect(networkUpdates.message1.securityInfo).toBe(securityInfo);
      expect(networkUpdates.message1.securityState).toBe("insecure");
    });

    it("adds fetched HTTP post data", () => {
      let requestPostData = {
        postData: {
          text: ""
        }
      };

      dispatch(actions.networkUpdateRequest("message1", {
        requestPostData: requestPostData
      }));

      let networkUpdates = getAllNetworkMessagesUpdateById(getState());
      expect(networkUpdates.message1.requestPostData).toBe(requestPostData);
      expect(networkUpdates.message1.requestHeadersFromUploadStream).toExist();
    });
  });
});
