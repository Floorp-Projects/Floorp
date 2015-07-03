/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

describe("loop.shared.views.TextChatView", function () {
  "use strict";

  var expect = chai.expect;
  var sharedActions = loop.shared.actions;
  var sharedViews = loop.shared.views;
  var TestUtils = React.addons.TestUtils;
  var CHAT_MESSAGE_TYPES = loop.store.CHAT_MESSAGE_TYPES;
  var CHAT_CONTENT_TYPES = loop.store.CHAT_CONTENT_TYPES;

  var dispatcher, fakeSdkDriver, sandbox, store, fakeClock;

  beforeEach(function() {
    sandbox = sinon.sandbox.create();
    fakeClock = sandbox.useFakeTimers();

    dispatcher = new loop.Dispatcher();
    sandbox.stub(dispatcher, "dispatch");

    fakeSdkDriver = {
      sendTextChatMessage: sinon.stub()
    };

    store = new loop.store.TextChatStore(dispatcher, {
      sdkDriver: fakeSdkDriver
    });

    loop.store.StoreMixin.register({
      textChatStore: store
    });
  });

  afterEach(function() {
    sandbox.restore();
  });

  describe("TextChatEntriesView", function() {
    var view;

    function mountTestComponent(extraProps) {
      var basicProps = {
        dispatcher: dispatcher,
        messageList: [],
        useDesktopPaths: false
      };

      return TestUtils.renderIntoDocument(
        React.createElement(loop.shared.views.chat.TextChatEntriesView,
          _.extend(basicProps, extraProps)));
    }

    it("should render message entries when message were sent/ received", function() {
      view = mountTestComponent({
        messageList: [{
          type: CHAT_MESSAGE_TYPES.RECEIVED,
          contentType: CHAT_CONTENT_TYPES.TEXT,
          message: "Hello!",
          receivedTimestamp: "2015-06-25T17:53:55.357Z"
        }, {
          type: CHAT_MESSAGE_TYPES.SENT,
          contentType: CHAT_CONTENT_TYPES.TEXT,
          message: "Is it me you're looking for?",
          sentTimestamp: "2015-06-25T17:53:55.357Z"
        }]
      });

      var node = view.getDOMNode();
      expect(node).to.not.eql(null);

      var entries = node.querySelectorAll(".text-chat-entry");
      expect(entries.length).to.eql(2);
      expect(entries[0].classList.contains("received")).to.eql(true);
      expect(entries[1].classList.contains("received")).to.not.eql(true);
    });

    it("should play a sound when a message is received", function() {
      view = mountTestComponent();
      sandbox.stub(view, "play");

      view.setProps({
        messageList: [{
          type: CHAT_MESSAGE_TYPES.RECEIVED,
          contentType: CHAT_CONTENT_TYPES.TEXT,
          message: "Hello!",
          receivedTimestamp: "2015-06-25T17:53:55.357Z"
        }]
      });

      sinon.assert.calledOnce(view.play);
      sinon.assert.calledWithExactly(view.play, "message");
    });

    it("should not play a sound when a special message is displayed", function() {
      view = mountTestComponent();
      sandbox.stub(view, "play");

      view.setProps({
        messageList: [{
          type: CHAT_MESSAGE_TYPES.SPECIAL,
          contentType: CHAT_CONTENT_TYPES.ROOM_NAME,
          message: "Hello!",
          receivedTimestamp: "2015-06-25T17:53:55.357Z"
        }]
      });

      sinon.assert.notCalled(view.play);
    });

    it("should not play a sound when a message is sent", function() {
      view = mountTestComponent();
      sandbox.stub(view, "play");

      view.setProps({
        messageList: [{
          type: CHAT_MESSAGE_TYPES.SENT,
          contentType: CHAT_CONTENT_TYPES.TEXT,
          message: "Hello!",
          sentTimestamp: "2015-06-25T17:53:55.357Z"
        }]
      });

      sinon.assert.notCalled(view.play);
    });
  });

  describe("TextChatEntry", function() {
    var view;

    function mountTestComponent(extraProps) {
      var props = _.extend({
        contentType: CHAT_CONTENT_TYPES.TEXT,
        dispatcher: dispatcher,
        message: "test",
        type: CHAT_MESSAGE_TYPES.RECEIVED,
        timestamp: "2015-06-23T22:48:39.738Z"
      }, extraProps);
      return TestUtils.renderIntoDocument(
        React.createElement(loop.shared.views.chat.TextChatEntry, props));
    }

    it("should not render a timestamp", function() {
      view = mountTestComponent({
        showTimestamp: false
      });
      var node = view.getDOMNode();

      expect(node.querySelector(".text-chat-entry-timestamp")).to.eql(null);
    });

    it("should render a timestamp", function() {
      view = mountTestComponent({
        showTimestamp: true
      });
      var node = view.getDOMNode();

      expect(node.querySelector(".text-chat-entry-timestamp")).to.not.eql(null);
    });
  });

  describe("TextChatEntriesView", function() {
    var view, node;

    function mountTestComponent(extraProps) {
      var props = _.extend({
        dispatcher: dispatcher,
        messageList: [],
        useDesktopPaths: false
      }, extraProps);
      return TestUtils.renderIntoDocument(
        React.createElement(loop.shared.views.chat.TextChatEntriesView, props));
    }

    beforeEach(function() {
      store.setStoreState({ textChatEnabled: true });
    });

    it("should show timestamps if there are different senders", function() {
      view = mountTestComponent({
        messageList: [{
          type: CHAT_MESSAGE_TYPES.RECEIVED,
          contentType: CHAT_CONTENT_TYPES.TEXT,
          message: "Hello!",
          receivedTimestamp: "2015-06-25T17:53:55.357Z"
        }, {
          type: CHAT_MESSAGE_TYPES.SENT,
          contentType: CHAT_CONTENT_TYPES.TEXT,
          message: "Is it me you're looking for?",
          sentTimestamp: "2015-06-25T17:53:55.357Z"
        }]
      });
      node = view.getDOMNode();

      expect(node.querySelectorAll(".text-chat-entry-timestamp").length)
          .to.eql(2);
    });

    it("should show timestamps if they are 1 minute apart (SENT)", function() {
      view = mountTestComponent({
        messageList: [{
          type: CHAT_MESSAGE_TYPES.SENT,
          contentType: CHAT_CONTENT_TYPES.TEXT,
          message: "Hello!",
          sentTimestamp: "2015-06-25T17:53:55.357Z"
        }, {
          type: CHAT_MESSAGE_TYPES.SENT,
          contentType: CHAT_CONTENT_TYPES.TEXT,
          message: "Is it me you're looking for?",
          sentTimestamp: "2015-06-25T17:54:55.357Z"
        }]
      });
      node = view.getDOMNode();

      expect(node.querySelectorAll(".text-chat-entry-timestamp").length)
          .to.eql(2);
    });

    it("should show timestamps if they are 1 minute apart (RECV)", function() {
      view = mountTestComponent({
        messageList: [{
          type: CHAT_MESSAGE_TYPES.RECEIVED,
          contentType: CHAT_CONTENT_TYPES.TEXT,
          message: "Hello!",
          receivedTimestamp: "2015-06-25T17:53:55.357Z"
        }, {
          type: CHAT_MESSAGE_TYPES.RECEIVED,
          contentType: CHAT_CONTENT_TYPES.TEXT,
          message: "Is it me you're looking for?",
          receivedTimestamp: "2015-06-25T17:54:55.357Z"
        }]
      });
      node = view.getDOMNode();

      expect(node.querySelectorAll(".text-chat-entry-timestamp").length)
          .to.eql(2);
    });

    it("should not show timestamps from msgs sent in the same minute", function() {
      view = mountTestComponent({
        messageList: [{
          type: CHAT_MESSAGE_TYPES.RECEIVED,
          contentType: CHAT_CONTENT_TYPES.TEXT,
          message: "Hello!",
          receivedTimestamp: "2015-06-25T17:53:55.357Z"
        }, {
          type: CHAT_MESSAGE_TYPES.RECEIVED,
          contentType: CHAT_CONTENT_TYPES.TEXT,
          message: "Is it me you're looking for?",
          sentTimestamp: "2015-06-25T17:53:55.357Z"
        }]
      });
      node = view.getDOMNode();

      expect(node.querySelectorAll(".text-chat-entry-timestamp").length)
          .to.eql(1);
    });
  });

  describe("TextChatView", function() {
    var view;

    function mountTestComponent(extraProps) {
      var props = _.extend({
        dispatcher: dispatcher,
        showRoomName: false,
        useDesktopPaths: false
      }, extraProps);
      return TestUtils.renderIntoDocument(
        React.createElement(loop.shared.views.chat.TextChatView, props));
    }

    beforeEach(function() {
      store.setStoreState({ textChatEnabled: true });
    });

    it("should show timestamps from msgs sent more than 1 min apart", function() {
      view = mountTestComponent();

      store.sendTextChatMessage({
        contentType: CHAT_CONTENT_TYPES.TEXT,
        message: "Hello!",
        sentTimestamp: "1970-01-01T00:02:00.000Z",
        receivedTimestamp: "1970-01-01T00:02:00.000Z"
      });

      store.sendTextChatMessage({
        contentType: CHAT_CONTENT_TYPES.TEXT,
        message: "Is it me you're looking for?",
        sentTimestamp: "1970-01-01T00:03:00.000Z",
        receivedTimestamp: "1970-01-01T00:03:00.000Z"
      });
      store.sendTextChatMessage({
        contentType: CHAT_CONTENT_TYPES.TEXT,
        message: "Is it me you're looking for?",
        sentTimestamp: "1970-01-01T00:02:00.000Z",
        receivedTimestamp: "1970-01-01T00:02:00.000Z"
      });

      var node = view.getDOMNode();

      expect(node.querySelectorAll(".text-chat-entry-timestamp").length)
          .to.eql(2);
    });

    it("should not display the view if no messages and text chat not enabled", function() {
      store.setStoreState({ textChatEnabled: false });

      view = mountTestComponent();

      expect(view.getDOMNode()).eql(null);
    });

    it("should display the view if no messages and text chat is enabled", function() {
      view = mountTestComponent();

      expect(view.getDOMNode()).not.eql(null);
    });

    it("should display only the text chat box if entry is enabled but there are no messages", function() {
      view = mountTestComponent();

      var node = view.getDOMNode();

      expect(node.querySelector(".text-chat-box")).not.eql(null);
      expect(node.querySelector(".text-chat-entries")).eql(null);
    });

    it("should render message entries when message were sent/ received", function() {
      view = mountTestComponent();

      store.receivedTextChatMessage({
        contentType: CHAT_CONTENT_TYPES.TEXT,
        message: "Hello!",
        sentTimestamp: "1970-01-01T00:03:00.000Z",
        receivedTimestamp: "1970-01-01T00:03:00.000Z"
      });
      store.sendTextChatMessage({
        contentType: CHAT_CONTENT_TYPES.TEXT,
        message: "Is it me you're looking for?",
        sentTimestamp: "1970-01-01T00:03:00.000Z",
        receivedTimestamp: "1970-01-01T00:03:00.000Z"
      });

      var node = view.getDOMNode();
      expect(node.querySelector(".text-chat-entries")).to.not.eql(null);

      var entries = node.querySelectorAll(".text-chat-entry");
      expect(entries.length).to.eql(2);
      expect(entries[0].classList.contains("received")).to.eql(true);
      expect(entries[1].classList.contains("received")).to.not.eql(true);
    });

    it("should add `sent` CSS class selector to msg of type SENT", function() {
      var node = mountTestComponent().getDOMNode();

      store.sendTextChatMessage({
        contentType: CHAT_CONTENT_TYPES.TEXT,
        message: "Foo",
        timestamp: 0
      });

      expect(node.querySelector(".sent")).to.not.eql(null);
    });

    it("should add `received` CSS class selector to msg of type RECEIVED",
      function() {
        var node = mountTestComponent().getDOMNode();

        store.receivedTextChatMessage({
          contentType: CHAT_CONTENT_TYPES.TEXT,
          message: "Foo",
          sentTimestamp: "1970-01-01T00:03:00.000Z",
          receivedTimestamp: "1970-01-01T00:03:00.000Z"
        });

        expect(node.querySelector(".received")).to.not.eql(null);
      });

    it("should render a room name special entry", function() {
      view = mountTestComponent({
        showRoomName: true
      });

      store.updateRoomInfo(new sharedActions.UpdateRoomInfo({
        roomName: "A wonderful surprise!",
        roomOwner: "Chris",
        roomUrl: "Fake"
      }));

      var node = view.getDOMNode();
      expect(node.querySelector(".text-chat-entries")).to.not.eql(null);

      var entries = node.querySelectorAll(".text-chat-entry");
      expect(entries.length).eql(1);
      expect(entries[0].classList.contains("special")).eql(true);
      expect(entries[0].classList.contains("room-name")).eql(true);
    });

    it("should render a special entry for the context url", function() {
      view = mountTestComponent();

      store.updateRoomInfo(new sharedActions.UpdateRoomInfo({
        roomName: "A Very Long Conversation Name",
        roomOwner: "fake",
        roomUrl: "http://showcase",
        urls: [{
          description: "A wonderful page!",
          location: "http://wonderful.invalid"
          // use the fallback thumbnail
        }]
      }));

      var node = view.getDOMNode();
      expect(node.querySelector(".text-chat-entries")).to.not.eql(null);

      expect(node.querySelector(".context-url-view-wrapper")).to.not.eql(null);
    });

    it("should dispatch SendTextChatMessage action when enter is pressed", function() {
      view = mountTestComponent();

      var entryNode = view.getDOMNode().querySelector(".text-chat-box > form > input");

      TestUtils.Simulate.change(entryNode, {
        target: {
          value: "Hello!"
        }
      });
      TestUtils.Simulate.keyDown(entryNode, {
        key: "Enter",
        which: 13
      });

      sinon.assert.calledOnce(dispatcher.dispatch);
      sinon.assert.calledWithExactly(dispatcher.dispatch,
        new sharedActions.SendTextChatMessage({
          contentType: CHAT_CONTENT_TYPES.TEXT,
          message: "Hello!",
          sentTimestamp: "1970-01-01T00:00:00.000Z"
        }));
    });

    it("should not dispatch SendTextChatMessage when the message is empty", function() {
      view = mountTestComponent();

      var entryNode = view.getDOMNode().querySelector(".text-chat-box > form > input");

      TestUtils.Simulate.keyDown(entryNode, {
        key: "Enter",
        which: 13
      });

      sinon.assert.notCalled(dispatcher.dispatch);
    });
  });
});
