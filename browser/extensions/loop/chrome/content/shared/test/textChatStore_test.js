/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

describe("loop.store.TextChatStore", function() {
  "use strict";

  var expect = chai.expect;
  var sharedActions = loop.shared.actions;
  var CHAT_MESSAGE_TYPES = loop.store.CHAT_MESSAGE_TYPES;
  var CHAT_CONTENT_TYPES = loop.shared.utils.CHAT_CONTENT_TYPES;

  var dispatcher, fakeSdkDriver, sandbox, store;

  beforeEach(function() {
    sandbox = sinon.sandbox.create();
    sandbox.useFakeTimers();

    dispatcher = new loop.Dispatcher();
    sandbox.stub(dispatcher, "dispatch");

    fakeSdkDriver = {
      sendTextChatMessage: sinon.stub()
    };

    store = new loop.store.TextChatStore(dispatcher, {
      sdkDriver: fakeSdkDriver
    });

    sandbox.stub(window, "dispatchEvent");
    sandbox.stub(window, "CustomEvent", function(name) {
      this.name = name;
    });
  });

  afterEach(function() {
    sandbox.restore();
  });

  describe("#dataChannelsAvailable", function() {
    it("should set textChatEnabled to the supplied state", function() {
      store.dataChannelsAvailable(new sharedActions.DataChannelsAvailable({
        available: true
      }));

      expect(store.getStoreState("textChatEnabled")).eql(true);
    });

    it("should dispatch a LoopChatEnabled event", function() {
      store.dataChannelsAvailable(new sharedActions.DataChannelsAvailable({
        available: true
      }));

      sinon.assert.calledOnce(window.dispatchEvent);
      sinon.assert.calledWithExactly(window.dispatchEvent,
        new CustomEvent("LoopChatEnabled"));
    });

    it("should not dispatch a LoopChatEnabled event if available is false", function() {
      store.dataChannelsAvailable(new sharedActions.DataChannelsAvailable({
        available: false
      }));

      sinon.assert.notCalled(window.dispatchEvent);
    });
  });

  describe("#receivedTextChatMessage", function() {
    it("should add the message to the list", function() {
      var message = "Hello!";

      store.receivedTextChatMessage({
        contentType: CHAT_CONTENT_TYPES.TEXT,
        message: message,
        extraData: undefined,
        sentTimestamp: "2015-06-24T23:58:53.848Z",
        receivedTimestamp: "1970-01-01T00:00:00.000Z"
      });

      expect(store.getStoreState("messageList")).eql([{
        type: CHAT_MESSAGE_TYPES.RECEIVED,
        contentType: CHAT_CONTENT_TYPES.TEXT,
        message: message,
        extraData: undefined,
        sentTimestamp: "2015-06-24T23:58:53.848Z",
        receivedTimestamp: "1970-01-01T00:00:00.000Z"
      }]);
    });

    it("should add the context tile to the list", function() {
      store.receivedTextChatMessage({
        type: CHAT_MESSAGE_TYPES.SENT,
        contentType: CHAT_CONTENT_TYPES.CONTEXT_TILE,
        message: "fake",
        extraData: {
          roomToken: "fakeRoomToken",
          newRoomThumbnail: "favicon",
          newRoomURL: "https://www.fakeurl.com"
        },
        sentTimestamp: "2015-06-24T23:58:53.848Z",
        receivedTimestamp: "1970-01-01T00:00:00.000Z"
      });

      expect(store.getStoreState("messageList")).eql([{
        type: CHAT_MESSAGE_TYPES.RECEIVED,
        contentType: CHAT_CONTENT_TYPES.CONTEXT_TILE,
        message: "fake",
        extraData: {
          roomToken: "fakeRoomToken",
          newRoomThumbnail: "favicon",
          newRoomURL: "https://www.fakeurl.com"
        },
        sentTimestamp: "2015-06-24T23:58:53.848Z",
        receivedTimestamp: "1970-01-01T00:00:00.000Z"
      }]);
    });

    it("should not add messages for unknown content types", function() {
      store.receivedTextChatMessage({
        contentType: "invalid type",
        message: "Hi"
      });

      expect(store.getStoreState("messageList").length).eql(0);
    });

    it("should dispatch a LoopChatMessageAppended event", function() {
      store.setStoreState({ textChatEnabled: true });
      store.receivedTextChatMessage({
        contentType: CHAT_CONTENT_TYPES.TEXT,
        message: "Hello!"
      });

      sinon.assert.calledOnce(window.dispatchEvent);
      sinon.assert.calledWithExactly(window.dispatchEvent,
        new CustomEvent("LoopChatMessageAppended"));
    });
  });

  describe("#sendTextChatMessage", function() {
    it("should send the message", function() {
      var messageData = {
        contentType: CHAT_CONTENT_TYPES.TEXT,
        message: "Yes, that's what this is called."
      };

      store.sendTextChatMessage(messageData);

      sinon.assert.calledOnce(fakeSdkDriver.sendTextChatMessage);
      sinon.assert.calledWithExactly(fakeSdkDriver.sendTextChatMessage, messageData);
    });

    it("should add the message to the list", function() {
      var messageData = {
        contentType: CHAT_CONTENT_TYPES.TEXT,
        message: "It's awesome!",
        sentTimestamp: "2015-06-24T23:58:53.848Z",
        receivedTimestamp: "2015-06-24T23:58:53.848Z"
      };

      store.sendTextChatMessage(messageData);

      expect(store.getStoreState("messageList")).eql([{
        type: CHAT_MESSAGE_TYPES.SENT,
        contentType: messageData.contentType,
        message: messageData.message,
        extraData: undefined,
        sentTimestamp: "2015-06-24T23:58:53.848Z",
        receivedTimestamp: "2015-06-24T23:58:53.848Z"
      }]);
    });

    it("should dipatch a LoopChatMessageAppended event", function() {
      store.setStoreState({ textChatEnabled: true });
      store.sendTextChatMessage({
        contentType: CHAT_CONTENT_TYPES.TEXT,
        message: "Hello!"
      });

      sinon.assert.calledOnce(window.dispatchEvent);
      sinon.assert.calledWithExactly(window.dispatchEvent,
        new CustomEvent("LoopChatMessageAppended"));
    });
  });

  describe("#updateRoomInfo", function() {
    it("should add the room name to the list", function() {
      store.updateRoomInfo(new sharedActions.UpdateRoomInfo({
        roomName: "Let's share!",
        roomUrl: "fake"
      }));

      expect(store.getStoreState("messageList")).eql([{
        type: CHAT_MESSAGE_TYPES.SPECIAL,
        contentType: CHAT_CONTENT_TYPES.ROOM_NAME,
        message: "Let's share!",
        extraData: undefined,
        sentTimestamp: undefined,
        receivedTimestamp: undefined
      }]);
    });

    it("should add the context to the list", function() {
      store.updateRoomInfo(new sharedActions.UpdateRoomInfo({
        roomName: "Let's share!",
        roomUrl: "fake",
        roomContextUrls: [{
          description: "A wonderful event",
          location: "http://wonderful.invalid",
          thumbnail: "fake"
        }]
      }));

      expect(store.getStoreState("messageList")).eql([
        {
          type: CHAT_MESSAGE_TYPES.SPECIAL,
          contentType: CHAT_CONTENT_TYPES.ROOM_NAME,
          message: "Let's share!",
          extraData: undefined,
          sentTimestamp: undefined,
          receivedTimestamp: undefined
        }, {
          type: CHAT_MESSAGE_TYPES.SPECIAL,
          contentType: CHAT_CONTENT_TYPES.CONTEXT,
          message: "A wonderful event",
          sentTimestamp: undefined,
          receivedTimestamp: undefined,
          extraData: {
            location: "http://wonderful.invalid",
            thumbnail: "fake"
          }
        }
      ]);
    });

    it("should not add more than one context message", function() {
      store.updateRoomInfo(new sharedActions.UpdateRoomInfo({
        roomUrl: "fake",
        roomContextUrls: [{
          description: "A wonderful event",
          location: "http://wonderful.invalid",
          thumbnail: "fake"
        }]
      }));

      expect(store.getStoreState("messageList")).eql([{
        type: CHAT_MESSAGE_TYPES.SPECIAL,
        contentType: CHAT_CONTENT_TYPES.CONTEXT,
        message: "A wonderful event",
        sentTimestamp: undefined,
        receivedTimestamp: undefined,
        extraData: {
          location: "http://wonderful.invalid",
          thumbnail: "fake"
        }
      }]);

      store.updateRoomInfo(new sharedActions.UpdateRoomInfo({
        roomUrl: "fake",
        roomContextUrls: [{
          description: "A wonderful event2",
          location: "http://wonderful.invalid2",
          thumbnail: "fake2"
        }]
      }));

      expect(store.getStoreState("messageList")).eql([{
        type: CHAT_MESSAGE_TYPES.SPECIAL,
        contentType: CHAT_CONTENT_TYPES.CONTEXT,
        message: "A wonderful event2",
        sentTimestamp: undefined,
        receivedTimestamp: undefined,
        extraData: {
          location: "http://wonderful.invalid2",
          thumbnail: "fake2"
        }
      }]);
    });

    it("should not dispatch a LoopChatMessageAppended event", function() {
      store.updateRoomInfo(new sharedActions.UpdateRoomInfo({
        roomName: "Let's share!",
        roomUrl: "fake"
      }));

      sinon.assert.notCalled(window.dispatchEvent);
    });
  });

  describe("#updateRoomContext", function() {
    beforeEach(function() {
      store.setStoreState({ messageList: [] });
      store.updateRoomContext(new sharedActions.UpdateRoomContext({
        newRoomDescription: "fake",
        newRoomThumbnail: "favicon",
        newRoomURL: "https://www.fakeurl.com",
        roomToken: "fakeRoomToken"
      }));
    });

    it("should add the room context to the list", function() {
      expect(store.getStoreState("messageList")).eql([{
        type: CHAT_MESSAGE_TYPES.SENT,
        contentType: CHAT_CONTENT_TYPES.CONTEXT_TILE,
        message: "fake",
        extraData: {
          roomToken: "fakeRoomToken",
          newRoomThumbnail: "favicon",
          newRoomURL: "https://www.fakeurl.com"
        },
        sentTimestamp: "1970-01-01T00:00:00.000Z",
        receivedTimestamp: undefined
      }]);
    });

    it("should not add the room context if the last tile has the same domain", function() {
      store.updateRoomContext(new sharedActions.UpdateRoomContext({
        newRoomDescription: "fake",
        newRoomThumbnail: "favicon",
        newRoomURL: "https://www.fakeurl.com/test/same_domain",
        roomToken: "fakeRoomToken"
      }));

      expect(store.getStoreState("messageList").length).eql(1);
    });

    it("should add the room context if the last tile has not the same domain", function() {
      store.updateRoomContext(new sharedActions.UpdateRoomContext({
        newRoomDescription: "fake",
        newRoomThumbnail: "favicon",
        newRoomURL: "https://www.myfakeurl.com",
        roomToken: "fakeRoomToken"
      }));

      expect(store.getStoreState("messageList").length).eql(2);
    });
  });

  describe("#remotePeerDisconnected", function() {

    it("should append the right message when peer disconnected cleanly", function() {
      store.remotePeerDisconnected(new sharedActions.RemotePeerDisconnected({
        peerHungup: true
      }));

      expect(store.getStoreState("messageList").length).eql(1);
      expect(store.getStoreState("messageList")[0].contentType).eql(
          CHAT_CONTENT_TYPES.NOTIFICATION
      );
      expect(store.getStoreState("messageList")[0].message).eql("peer_left_session");
    });

    it("should append the right message when peer disconnected unexpectedly", function() {
      store.remotePeerDisconnected(new sharedActions.RemotePeerDisconnected({
        peerHungup: false
      }));

      expect(store.getStoreState("messageList").length).eql(1);
      expect(store.getStoreState("messageList")[0].contentType).eql(
          CHAT_CONTENT_TYPES.NOTIFICATION
      );
      expect(store.getStoreState("messageList")[0].message).eql("peer_unexpected_quit");
    });
  });
});
