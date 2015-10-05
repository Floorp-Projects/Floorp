/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

describe("loop.shared.views.TextChatView", function () {
  "use strict";

  var expect = chai.expect;
  var sharedActions = loop.shared.actions;
  var sharedViews = loop.shared.views;
  var TestUtils = React.addons.TestUtils;
  var CHAT_MESSAGE_TYPES = loop.store.CHAT_MESSAGE_TYPES;
  var CHAT_CONTENT_TYPES = loop.shared.utils.CHAT_CONTENT_TYPES;
  var fixtures = document.querySelector("#fixtures");

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
    React.unmountComponentAtNode(fixtures);
  });

  describe("TextChatEntriesView", function() {
    var view, node;

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

    function mountAsRealComponent(extraProps, container) {
      var basicProps = {
        dispatcher: dispatcher,
        messageList: [],
        useDesktopPaths: false
      };

      return React.render(
        React.createElement(loop.shared.views.chat.TextChatEntriesView,
          _.extend(basicProps, extraProps)), container);
    }

    beforeEach(function() {
      store.setStoreState({ textChatEnabled: true });
    });

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

      node = view.getDOMNode();
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
          receivedTimestamp: "2015-06-25T17:53:55.357Z"
        }]
      });
      node = view.getDOMNode();

      expect(node.querySelectorAll(".text-chat-entry-timestamp").length)
          .to.eql(1);
    });

    describe("Scrolling", function() {
      beforeEach(function() {
        sandbox.stub(window, "requestAnimationFrame", function(callback) {
          callback();
        });

        // We're using scrolling, so we need to mount as a real one.
        view = mountAsRealComponent({}, fixtures);
        sandbox.stub(view, "play");

        // We need some basic styling to ensure scrolling.
        view.getDOMNode().style.overflow = "scroll";
        view.getDOMNode().style["max-height"] = "4ch";
      });

      it("should scroll when a text message is added", function() {
        var messageList = [{
          type: CHAT_MESSAGE_TYPES.RECEIVED,
          contentType: CHAT_CONTENT_TYPES.TEXT,
          message: "Hello!",
          receivedTimestamp: "2015-06-25T17:53:55.357Z"
        }];

        view.setProps({ messageList: messageList });

        node = view.getDOMNode();

        expect(node.scrollTop).eql(node.scrollHeight - node.clientHeight);
      });

      it("should not scroll when a context tile is added", function() {
        var messageList = [{
          type: CHAT_MESSAGE_TYPES.SPECIAL,
          contentType: CHAT_CONTENT_TYPES.CONTEXT,
          message: "Awesome!",
          extraData: {
            location: "http://invalid.com"
          }
        }];

        view.setProps({ messageList: messageList });

        node = view.getDOMNode();

        expect(node.scrollTop).eql(0);
      });

      it("should scroll when a message is received after a context tile", function() {
        // The context tile.
        var messageList = [{
          type: CHAT_MESSAGE_TYPES.SPECIAL,
          contentType: CHAT_CONTENT_TYPES.CONTEXT,
          message: "Awesome!",
          extraData: {
            location: "http://invalid.com"
          }
        }];

        view.setProps({ messageList: messageList });

        // Now add a message. Don't use the same list as this is a shared object,
        // that messes with React.
        var messageList1 = [
          messageList[0], {
            type: CHAT_MESSAGE_TYPES.RECEIVED,
            contentType: CHAT_CONTENT_TYPES.TEXT,
            message: "Hello!",
            receivedTimestamp: "2015-06-25T17:53:55.357Z"
          }
        ];

        view.setProps({ messageList: messageList1 });

        node = view.getDOMNode();

        expect(node.scrollTop).eql(node.scrollHeight - node.clientHeight);

      });

      it("should not scroll when receiving a message and the scroll is not at the bottom", function() {
        node = view.getDOMNode();

        var messageList = [{
          type: CHAT_MESSAGE_TYPES.RECEIVED,
          contentType: CHAT_CONTENT_TYPES.TEXT,
          message: "Hello!",
          receivedTimestamp: "2015-06-25T17:53:55.357Z"
        }];

        view.setProps({ messageList: messageList });

        node.scrollTop = 0;

        // Don't use the same list as this is a shared object, that messes with React.
        var messageList1 = [
          messageList[0], {
            type: CHAT_MESSAGE_TYPES.RECEIVED,
            contentType: CHAT_CONTENT_TYPES.TEXT,
            message: "Hello!",
            receivedTimestamp: "2015-06-25T17:53:55.357Z"
          }
        ];

        view.setProps({ messageList: messageList1 });

        expect(node.scrollTop).eql(0);
      });
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
        showTimestamp: false,
        timestamp: "2015-06-23T22:48:39.738Z",
        type: CHAT_MESSAGE_TYPES.RECEIVED,
        contentType: CHAT_CONTENT_TYPES.TEXT,
        message: "foo"
      });
      var node = view.getDOMNode();

      expect(node.querySelector(".text-chat-entry-timestamp")).to.eql(null);
    });

    it("should render a timestamp", function() {
      view = mountTestComponent({
        showTimestamp: true,
        timestamp: "2015-06-23T22:48:39.738Z",
        type: CHAT_MESSAGE_TYPES.RECEIVED,
        contentType: CHAT_CONTENT_TYPES.TEXT,
        message: "foo"
      });
      var node = view.getDOMNode();

      expect(node.querySelector(".text-chat-entry-timestamp")).to.not.eql(null);
    });

    // note that this is really an integration test to be sure that we don't
    // inadvertently regress using LinkifiedTextView.
    it("should linkify a URL starting with http", function (){
      view = mountTestComponent({
        showTimestamp: true,
        timestamp: "2015-06-23T22:48:39.738Z",
        type: CHAT_MESSAGE_TYPES.RECEIVED,
        contentType: CHAT_CONTENT_TYPES.TEXT,
        message: "Check out http://example.com and see what you think..."
      });
      var node = view.getDOMNode();

      expect(node.querySelector("a")).to.not.eql(null);
    });
  });

  describe("TextChatView", function() {
    var view, fakeServer;

    function mountTestComponent(extraProps) {
      var props = _.extend({
        dispatcher: dispatcher,
        showRoomName: false,
        useDesktopPaths: false,
        showAlways: true
      }, extraProps);
      return TestUtils.renderIntoDocument(
        React.createElement(loop.shared.views.chat.TextChatView, props));
    }

    beforeEach(function() {
      // Fake server to catch all XHR requests.
      fakeServer = sinon.fakeServer.create();
      store.setStoreState({ textChatEnabled: true });

      sandbox.stub(navigator.mozL10n, "get", function(string) {
        return string;
      });
    });

    afterEach(function() {
      fakeServer.restore();
    });

    it("should add a disabled class when text chat is disabled", function() {
      view = mountTestComponent();

      store.setStoreState({ textChatEnabled: false });

      expect(view.getDOMNode().classList.contains("text-chat-disabled")).eql(true);
    });

    it("should not a disabled class when text chat is enabled", function() {
      view = mountTestComponent();

      store.setStoreState({ textChatEnabled: true });

      expect(view.getDOMNode().classList.contains("text-chat-disabled")).eql(false);
    });

    it("should add an empty class when the entries list is empty", function() {
      view = mountTestComponent();

      expect(view.getDOMNode().classList.contains("text-chat-entries-empty")).eql(true);
    });

    it("should not add an empty class when the entries list is has items", function() {
      view = mountTestComponent();

      store.sendTextChatMessage({
        contentType: CHAT_CONTENT_TYPES.TEXT,
        message: "Hello!",
        sentTimestamp: "1970-01-01T00:02:00.000Z",
        receivedTimestamp: "1970-01-01T00:02:00.000Z"
      });

      expect(view.getDOMNode().classList.contains("text-chat-entries-empty")).eql(false);
    });

    it("should add a showing room name class when the view shows room names and it has a room name", function() {
      view = mountTestComponent({
        showRoomName: true
      });

      store.updateRoomInfo(new sharedActions.UpdateRoomInfo({
        roomName: "Study",
        roomUrl: "Fake"
      }));

      expect(view.getDOMNode().classList.contains("showing-room-name")).eql(true);
    });

    it("shouldn't add a showing room name class when the view doesn't show room names", function() {
      view = mountTestComponent({
        showRoomName: false
      });

      store.updateRoomInfo(new sharedActions.UpdateRoomInfo({
        roomName: "Study",
        roomUrl: "Fake"
      }));

      expect(view.getDOMNode().classList.contains("showing-room-name")).eql(false);
    });

    it("shouldn't add a showing room name class when the view doesn't have a name", function() {
      view = mountTestComponent({
        showRoomName: true
      });

      expect(view.getDOMNode().classList.contains("showing-room-name")).eql(false);
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
        sentTimestamp: "2015-06-25T17:53:55.357Z"
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
        roomUrl: "Fake"
      }));

      var node = view.getDOMNode();
      expect(node.querySelector(".text-chat-entries")).to.not.eql(null);

      var entries = node.querySelectorAll(".text-chat-header");
      expect(entries.length).eql(1);
      expect(entries[0].classList.contains("special")).eql(true);
      expect(entries[0].classList.contains("room-name")).eql(true);
    });

    it("should render a special entry for the context url", function() {
      view = mountTestComponent();

      store.updateRoomInfo(new sharedActions.UpdateRoomInfo({
        roomName: "A Very Long Conversation Name",
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

    it("should show a placeholder when no messages have been sent", function() {
      view = mountTestComponent();

      store.receivedTextChatMessage({
        contentType: CHAT_CONTENT_TYPES.TEXT,
        message: "Foo",
        sentTimestamp: "1970-01-01T00:03:00.000Z",
        receivedTimestamp: "1970-01-01T00:03:00.000Z"
      });

      var textBox = view.getDOMNode().querySelector(".text-chat-box input");

      expect(textBox.placeholder).contain("placeholder");
    });

    it("should not show a placeholder when messages have been sent", function() {
      view = mountTestComponent();

      store.sendTextChatMessage({
        contentType: CHAT_CONTENT_TYPES.TEXT,
        message: "Foo",
        sentTimestamp: "2015-06-25T17:53:55.357Z"
      });

      var textBox = view.getDOMNode().querySelector(".text-chat-box input");

      expect(textBox.placeholder).not.contain("placeholder");
    });
  });
});
