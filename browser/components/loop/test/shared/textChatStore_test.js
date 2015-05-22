/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

describe("loop.store.TextChatStore", function () {
  "use strict";

  var expect = chai.expect;
  var sharedActions = loop.shared.actions;
  var CHAT_MESSAGE_TYPES = loop.store.CHAT_MESSAGE_TYPES;
  var CHAT_CONTENT_TYPES = loop.store.CHAT_CONTENT_TYPES;

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
  });

  afterEach(function() {
    sandbox.restore();
  });

  describe("#dataChannelsAvailable", function() {
    it("should set textChatEnabled to true", function() {
      store.dataChannelsAvailable();

      expect(store.getStoreState("textChatEnabled")).eql(true);
    });
  });

  describe("#receivedTextChatMessage", function() {
    it("should add the message to the list", function() {
      var message = "Hello!";

      store.receivedTextChatMessage({
        contentType: CHAT_CONTENT_TYPES.TEXT,
        message: message
      });

      expect(store.getStoreState("messageList")).eql([{
        type: CHAT_MESSAGE_TYPES.RECEIVED,
        contentType: CHAT_CONTENT_TYPES.TEXT,
        message: message
      }]);
    });

    it("should not add messages for unknown content types", function() {
      store.receivedTextChatMessage({
        contentType: "invalid type",
        message: "Hi"
      });

      expect(store.getStoreState("messageList").length).eql(0);
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
        message: "It's awesome!"
      };

      store.sendTextChatMessage(messageData);

      expect(store.getStoreState("messageList")).eql([{
        type: CHAT_MESSAGE_TYPES.SENT,
        contentType: messageData.contentType,
        message: messageData.message
      }]);
    });
  });
});
