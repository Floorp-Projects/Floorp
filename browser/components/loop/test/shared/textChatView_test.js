/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

describe("loop.shared.views.TextChatView", function () {
  "use strict";

  var expect = chai.expect;
  var sharedActions = loop.shared.actions;
  var TestUtils = React.addons.TestUtils;
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

    loop.store.StoreMixin.register({
      textChatStore: store
    });
  });

  afterEach(function() {
    sandbox.restore();
  });

  describe("TextChatView", function() {
    var view;

    function mountTestComponent(extraProps) {
      var props = _.extend({
        dispatcher: dispatcher
      }, extraProps);
      return TestUtils.renderIntoDocument(
        React.createElement(loop.shared.views.TextChatView, props));
    }

    beforeEach(function() {
      store.setStoreState({ textChatEnabled: true });
    });

    it("should not display anything if no messages and text chat not enabled and showAlways is false", function() {
      store.setStoreState({ textChatEnabled: false });

      view = mountTestComponent({
        showAlways: false
      });

      expect(view.getDOMNode()).eql(null);
    });

    it("should display the view if no messages and text chat not enabled and showAlways is true", function() {
      store.setStoreState({ textChatEnabled: false });

      view = mountTestComponent({
        showAlways: true
      });

      expect(view.getDOMNode()).not.eql(null);
    });

    it("should display the view if text chat is enabled", function() {
      view = mountTestComponent({
        showAlways: true
      });

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
        message: "Hello!"
      });
      store.sendTextChatMessage({
        contentType: CHAT_CONTENT_TYPES.TEXT,
        message: "Is it me you're looking for?"
      });

      var node = view.getDOMNode();
      expect(node.querySelector(".text-chat-entries")).to.not.eql(null);

      var entries = node.querySelectorAll(".text-chat-entry");
      expect(entries.length).to.eql(2);
      expect(entries[0].classList.contains("received")).to.eql(true);
      expect(entries[1].classList.contains("received")).to.not.eql(true);
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
          message: "Hello!"
        }));
    });
  });
});
