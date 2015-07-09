/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

describe("loop.store.ConversationAppStore", function () {
  "use strict";

  var expect = chai.expect;
  var sharedActions = loop.shared.actions;
  var sandbox, dispatcher;

  beforeEach(function() {
    sandbox = sinon.sandbox.create();
    dispatcher = new loop.Dispatcher();
  });

  afterEach(function() {
    sandbox.restore();
  });

  describe("#constructor", function() {
    it("should throw an error if the dispatcher is missing", function() {
      expect(function() {
        new loop.store.ConversationAppStore({mozLoop: {}});
      }).to.Throw(/dispatcher/);
    });

    it("should throw an error if mozLoop is missing", function() {
      expect(function() {
        new loop.store.ConversationAppStore({dispatcher: dispatcher});
      }).to.Throw(/mozLoop/);
    });
  });

  describe("#getWindowData", function() {
    var fakeWindowData, fakeGetWindowData, fakeMozLoop, store;

    beforeEach(function() {
      fakeWindowData = {
        type: "incoming",
        callId: "123456"
      };

      fakeGetWindowData = {
        windowId: "42"
      };

      fakeMozLoop = {
        getConversationWindowData: function(windowId) {
          if (windowId === "42") {
            return fakeWindowData;
          }
          return null;
        }
      };

      store = new loop.store.ConversationAppStore({
        dispatcher: dispatcher,
        mozLoop: fakeMozLoop
      });
    });

    it("should fetch the window type from the mozLoop API", function() {
      dispatcher.dispatch(new sharedActions.GetWindowData(fakeGetWindowData));

      expect(store.getStoreState()).eql({
        windowType: "incoming"
      });
    });

    it("should dispatch a SetupWindowData action with the data from the mozLoop API",
      function() {
        sandbox.stub(dispatcher, "dispatch");

        store.getWindowData(new sharedActions.GetWindowData(fakeGetWindowData));

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.SetupWindowData(_.extend({
            windowId: fakeGetWindowData.windowId
          }, fakeWindowData)));
      });
  });

});
