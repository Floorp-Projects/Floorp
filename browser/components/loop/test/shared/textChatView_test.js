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

    function mountTestComponent() {
      return TestUtils.renderIntoDocument(
        React.createElement(loop.shared.views.TextChatView, {
          dispatcher: dispatcher
        }));
    }

    beforeEach(function() {
      store.setStoreState({ textChatEnabled: true });
    });

    it("should not display anything if text chat is disabled", function() {
      store.setStoreState({ textChatEnabled: false });

      view = mountTestComponent();

      expect(view.getDOMNode()).eql(null);
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
