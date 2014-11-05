/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

var expect = chai.expect;

describe("loop.store.StandaloneAppStore", function () {
  var sharedActions = loop.shared.actions;
  var sharedUtils = loop.shared.utils;
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
        new loop.store.StandaloneAppStore({
          sdk: {},
          helper: {},
          conversation: {}
        });
      }).to.Throw(/dispatcher/);
    });

    it("should throw an error if sdk is missing", function() {
      expect(function() {
        new loop.store.StandaloneAppStore({
          dispatcher: dispatcher,
          helper: {},
          conversation: {}
        });
      }).to.Throw(/sdk/);
    });

    it("should throw an error if helper is missing", function() {
      expect(function() {
        new loop.store.StandaloneAppStore({
          dispatcher: dispatcher,
          sdk: {},
          conversation: {}
        });
      }).to.Throw(/helper/);
    });

    it("should throw an error if conversation is missing", function() {
      expect(function() {
        new loop.store.StandaloneAppStore({
          dispatcher: dispatcher,
          sdk: {},
          helper: {}
        });
      }).to.Throw(/conversation/);
    });
  });

  describe("#extractTokenInfo", function() {
    var store, fakeGetWindowData, fakeSdk, fakeConversation, helper;

    beforeEach(function() {
      fakeGetWindowData = {
        windowPath: ""
      };

      helper = new sharedUtils.Helper();
      sandbox.stub(helper, "isIOS").returns(false);

      fakeSdk = {
        checkSystemRequirements: sinon.stub().returns(true)
      };

      fakeConversation = {
        set: sinon.spy()
      };

      sandbox.stub(dispatcher, "dispatch");

      store = new loop.store.StandaloneAppStore({
        dispatcher: dispatcher,
        sdk: fakeSdk,
        helper: helper,
        conversation: fakeConversation
      });
    });

    it("should set windowType to `unsupportedDevice` for IOS", function() {
      // The stub should return true for this test.
      helper.isIOS.returns(true);

      store.extractTokenInfo(
        new sharedActions.ExtractTokenInfo(fakeGetWindowData));

      expect(store.getStoreState()).eql({
        windowType: "unsupportedDevice"
      });
    });

    it("should set windowType to `unsupportedBrowser` for browsers the sdk does not support",
      function() {
        // The stub should return false for this test.
        fakeSdk.checkSystemRequirements.returns(false);

        store.extractTokenInfo(
          new sharedActions.ExtractTokenInfo(fakeGetWindowData));

        expect(store.getStoreState()).eql({
          windowType: "unsupportedBrowser"
        });
      });

    it("should set windowType to `outgoing` for old style call hashes", function() {
      fakeGetWindowData.windowPath = "#call/faketoken";

      store.extractTokenInfo(
        new sharedActions.ExtractTokenInfo(fakeGetWindowData));

      expect(store.getStoreState()).eql({
        windowType: "outgoing"
      });
    });

    it("should set windowType to `outgoing` for new style call paths", function() {
      fakeGetWindowData.windowPath = "/c/fakecalltoken";

      store.extractTokenInfo(
        new sharedActions.ExtractTokenInfo(fakeGetWindowData));

      expect(store.getStoreState()).eql({
        windowType: "outgoing"
      });
    });

    it("should set windowType to `room` for room paths", function() {
      fakeGetWindowData.windowPath = "/fakeroomtoken";

      store.extractTokenInfo(
        new sharedActions.ExtractTokenInfo(fakeGetWindowData));

      expect(store.getStoreState()).eql({
        windowType: "room"
      });
    });

    it("should set windowType to `home` for unknown paths", function() {
      fakeGetWindowData.windowPath = "/";

      store.extractTokenInfo(
        new sharedActions.ExtractTokenInfo(fakeGetWindowData));

      expect(store.getStoreState()).eql({
        windowType: "home"
      });
    });

    it("should set the loopToken on the conversation for old style call hashes",
      function() {
        fakeGetWindowData.windowPath = "#call/faketoken";

        store.extractTokenInfo(
          new sharedActions.ExtractTokenInfo(fakeGetWindowData));

        sinon.assert.calledOnce(fakeConversation.set);
        sinon.assert.calledWithExactly(fakeConversation.set, {
          loopToken: "faketoken"
        });
      });

    it("should set the loopToken on the conversation for new style call paths",
      function() {
        fakeGetWindowData.windowPath = "/c/fakecalltoken";

        store.extractTokenInfo(
          new sharedActions.ExtractTokenInfo(fakeGetWindowData));

        sinon.assert.calledOnce(fakeConversation.set);
        sinon.assert.calledWithExactly(fakeConversation.set, {
          loopToken: "fakecalltoken"
        });
      });

    it("should set the loopToken on the conversation for room paths",
      function() {
        fakeGetWindowData.windowPath = "/c/fakeroomtoken";

        store.extractTokenInfo(
          new sharedActions.ExtractTokenInfo(fakeGetWindowData));

        sinon.assert.calledOnce(fakeConversation.set);
        sinon.assert.calledWithExactly(fakeConversation.set, {
          loopToken: "fakeroomtoken"
        });
      });

    it("should dispatch a SetupWindowData action for old style call hashes",
      function() {
        fakeGetWindowData.windowPath = "#call/faketoken";

        store.extractTokenInfo(
          new sharedActions.ExtractTokenInfo(fakeGetWindowData));

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.FetchServerData({
            windowType: "outgoing",
            token: "faketoken"
          }));
      });

    it("should set the loopToken on the conversation for new style call paths",
      function() {
        fakeGetWindowData.windowPath = "/c/fakecalltoken";

        store.extractTokenInfo(
          new sharedActions.ExtractTokenInfo(fakeGetWindowData));

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.FetchServerData({
            windowType: "outgoing",
            token: "fakecalltoken"
          }));
      });

    it("should set the loopToken on the conversation for room paths",
      function() {
        fakeGetWindowData.windowPath = "/c/fakeroomtoken";

        store.extractTokenInfo(
          new sharedActions.ExtractTokenInfo(fakeGetWindowData));

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.FetchServerData({
            windowType: "outgoing",
            token: "fakeroomtoken"
          }));
      });

  });

});
