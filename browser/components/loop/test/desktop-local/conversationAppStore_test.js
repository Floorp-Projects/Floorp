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
    var fakeWindowData, fakeGetWindowData, fakeMozLoop, store, getLoopPrefStub;
    var setLoopPrefStub;

    beforeEach(function() {
      fakeWindowData = {
        type: "incoming",
        callId: "123456"
      };

      fakeGetWindowData = {
        windowId: "42"
      };

      getLoopPrefStub = sandbox.stub();
      setLoopPrefStub = sandbox.stub();

      fakeMozLoop = {
        getConversationWindowData: function(windowId) {
          if (windowId === "42") {
            return fakeWindowData;
          }
          return null;
        },
        getLoopPref: getLoopPrefStub,
        setLoopPref: setLoopPrefStub
      };

      store = new loop.store.ConversationAppStore({
        dispatcher: dispatcher,
        mozLoop: fakeMozLoop
      });
    });

    afterEach(function() {
      sandbox.restore();
    });

    it("should fetch the window type from the mozLoop API", function() {
      dispatcher.dispatch(new sharedActions.GetWindowData(fakeGetWindowData));

      expect(store.getStoreState()).eql({
        windowType: "incoming"
      });
    });

    it("should have the feedback period in initial state", function() {
      getLoopPrefStub.returns(42);

      // Expect ms.
      expect(store.getInitialStoreState().feedbackPeriod).to.eql(42 * 1000);
    });

    it("should have the dateLastSeen in initial state", function() {
      getLoopPrefStub.returns(42);

      // Expect ms.
      expect(store.getInitialStoreState().feedbackTimestamp).to.eql(42 * 1000);
    });

    it("should fetch the correct pref for feedback period", function() {
      store.getInitialStoreState();

      sinon.assert.calledWithExactly(getLoopPrefStub, "feedback.periodSec");
    });

    it("should fetch the correct pref for feedback period", function() {
      store.getInitialStoreState();

      sinon.assert.calledWithExactly(getLoopPrefStub,
                                     "feedback.dateLastSeenSec");
    });

    it("should set showFeedbackForm to true when action is triggered", function() {
      var showFeedbackFormStub = sandbox.stub(store, "showFeedbackForm");

      dispatcher.dispatch(new sharedActions.ShowFeedbackForm());

      sinon.assert.calledOnce(showFeedbackFormStub);
    });

    it("should set feedback timestamp on ShowFeedbackForm action", function() {
      var clock = sandbox.useFakeTimers();
      // Make sure we round down the value.
      clock.tick(1001);
      dispatcher.dispatch(new sharedActions.ShowFeedbackForm());

      sinon.assert.calledOnce(setLoopPrefStub);
      sinon.assert.calledWithExactly(setLoopPrefStub,
                                     "feedback.dateLastSeenSec", 1);
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
