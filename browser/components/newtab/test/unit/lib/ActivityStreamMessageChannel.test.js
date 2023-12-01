import {
  actionCreators as ac,
  actionTypes as at,
} from "common/Actions.sys.mjs";
import {
  ActivityStreamMessageChannel,
  DEFAULT_OPTIONS,
} from "lib/ActivityStreamMessageChannel.jsm";
import { addNumberReducer, GlobalOverrider } from "test/unit/utils";
import { applyMiddleware, createStore } from "redux";

const OPTIONS = [
  "pageURL, outgoingMessageName",
  "incomingMessageName",
  "dispatch",
];

// Create an object containing details about a tab as expected within
// the loaded tabs map in ActivityStreamMessageChannel.jsm.
function getTabDetails(portID, url = "about:newtab", extraArgs = {}) {
  let actor = {
    portID,
    sendAsyncMessage: sinon.spy(),
  };
  let browser = {
    getAttribute: () => (extraArgs.preloaded ? "preloaded" : ""),
    ownerGlobal: {},
  };
  let browsingContext = {
    top: {
      embedderElement: browser,
    },
  };

  let data = {
    data: {
      actor,
      browser,
      browsingContext,
      portID,
      url,
    },
    target: {
      browsingContext,
    },
  };

  if (extraArgs.loaded) {
    data.data.loaded = extraArgs.loaded;
  }
  if (extraArgs.simulated) {
    data.data.simulated = extraArgs.simulated;
  }

  return data;
}

describe("ActivityStreamMessageChannel", () => {
  let globals;
  let dispatch;
  let mm;
  beforeEach(() => {
    globals = new GlobalOverrider();
    globals.set("AboutNewTab", {
      reset: globals.sandbox.spy(),
    });
    globals.set("AboutHomeStartupCache", { onPreloadedNewTabMessage() {} });
    globals.set("AboutNewTabParent", {
      flushQueuedMessagesFromContent: globals.sandbox.stub(),
    });

    dispatch = globals.sandbox.spy();
    mm = new ActivityStreamMessageChannel({ dispatch });

    assert.ok(mm.loadedTabs, []);

    let loadedTabs = new Map();
    let sandbox = sinon.createSandbox();
    sandbox.stub(mm, "loadedTabs").get(() => loadedTabs);
  });

  afterEach(() => globals.restore());

  describe("portID validation", () => {
    let sandbox;
    beforeEach(() => {
      sandbox = sinon.createSandbox();
      sandbox.spy(global.console, "error");
    });
    afterEach(() => {
      sandbox.restore();
    });
    it("should log errors for an invalid portID", () => {
      mm.validatePortID({});
      mm.validatePortID({});
      mm.validatePortID({});

      assert.equal(global.console.error.callCount, 3);
    });
  });

  it("should exist", () => {
    assert.ok(ActivityStreamMessageChannel);
  });
  it("should apply default options", () => {
    mm = new ActivityStreamMessageChannel();
    OPTIONS.forEach(o => assert.equal(mm[o], DEFAULT_OPTIONS[o], o));
  });
  it("should add options", () => {
    const options = {
      dispatch: () => {},
      pageURL: "FOO.html",
      outgoingMessageName: "OUT",
      incomingMessageName: "IN",
    };
    mm = new ActivityStreamMessageChannel(options);
    OPTIONS.forEach(o => assert.equal(mm[o], options[o], o));
  });
  it("should throw an error if no dispatcher was provided", () => {
    mm = new ActivityStreamMessageChannel();
    assert.throws(() => mm.dispatch({ type: "FOO" }));
  });
  describe("Creating/destroying the channel", () => {
    describe("#simulateMessagesForExistingTabs", () => {
      beforeEach(() => {
        sinon.stub(mm, "onActionFromContent");
      });
      it("should simulate init for existing ports", () => {
        let msg1 = getTabDetails("inited", "about:monkeys", {
          simulated: true,
        });
        mm.loadedTabs.set(msg1.data.browser, msg1.data);

        let msg2 = getTabDetails("loaded", "about:sheep", {
          simulated: true,
        });
        mm.loadedTabs.set(msg2.data.browser, msg2.data);

        mm.simulateMessagesForExistingTabs();

        assert.calledWith(mm.onActionFromContent.firstCall, {
          type: at.NEW_TAB_INIT,
          data: msg1.data,
        });
        assert.calledWith(mm.onActionFromContent.secondCall, {
          type: at.NEW_TAB_INIT,
          data: msg2.data,
        });
      });
      it("should simulate load for loaded ports", () => {
        let msg3 = getTabDetails("foo", null, {
          preloaded: true,
          loaded: true,
        });
        mm.loadedTabs.set(msg3.data.browser, msg3.data);

        mm.simulateMessagesForExistingTabs();

        assert.calledWith(
          mm.onActionFromContent,
          { type: at.NEW_TAB_LOAD },
          "foo"
        );
      });
      it("should set renderLayers on preloaded browsers after load", () => {
        let msg4 = getTabDetails("foo", null, {
          preloaded: true,
          loaded: true,
        });
        msg4.data.browser.ownerGlobal = {
          STATE_MAXIMIZED: 1,
          STATE_MINIMIZED: 2,
          STATE_NORMAL: 3,
          STATE_FULLSCREEN: 4,
          windowState: 3,
          isFullyOccluded: false,
        };
        mm.loadedTabs.set(msg4.data.browser, msg4.data);
        mm.simulateMessagesForExistingTabs();
        assert.equal(msg4.data.browser.renderLayers, true);
      });
      it("should flush queued messages from content when doing the simulation", () => {
        assert.notCalled(
          global.AboutNewTabParent.flushQueuedMessagesFromContent
        );
        mm.simulateMessagesForExistingTabs();
        assert.calledOnce(
          global.AboutNewTabParent.flushQueuedMessagesFromContent
        );
      });
    });
  });
  describe("Message handling", () => {
    describe("#getTargetById", () => {
      it("should get an id if it exists", () => {
        let msg = getTabDetails("foo:1");
        mm.loadedTabs.set(msg.data.browser, msg.data);
        assert.equal(mm.getTargetById("foo:1"), msg.data.actor);
      });
      it("should return null if the target doesn't exist", () => {
        let msg = getTabDetails("foo:2");
        mm.loadedTabs.set(msg.data.browser, msg.data);
        assert.equal(mm.getTargetById("bar:3"), null);
      });
    });
    describe("#getPreloadedActors", () => {
      it("should get a preloaded actor if it exists", () => {
        let msg = getTabDetails("foo:3", null, { preloaded: true });
        mm.loadedTabs.set(msg.data.browser, msg.data);
        assert.equal(mm.getPreloadedActors()[0].portID, "foo:3");
      });
      it("should get all the preloaded actors across windows if they exist", () => {
        let msg = getTabDetails("foo:4a", null, { preloaded: true });
        mm.loadedTabs.set(msg.data.browser, msg.data);
        msg = getTabDetails("foo:4b", null, { preloaded: true });
        mm.loadedTabs.set(msg.data.browser, msg.data);
        assert.equal(mm.getPreloadedActors().length, 2);
      });
      it("should return null if there is no preloaded actor", () => {
        let msg = getTabDetails("foo:5");
        mm.loadedTabs.set(msg.data.browser, msg.data);
        assert.equal(mm.getPreloadedActors(), null);
      });
    });
    describe("#onNewTabInit", () => {
      it("should dispatch a NEW_TAB_INIT action", () => {
        let msg = getTabDetails("foo", "about:monkeys");
        sinon.stub(mm, "onActionFromContent");

        mm.onNewTabInit(msg, msg.data);

        assert.calledWith(mm.onActionFromContent, {
          type: at.NEW_TAB_INIT,
          data: msg.data,
        });
      });
    });
    describe("#onNewTabLoad", () => {
      it("should dispatch a NEW_TAB_LOAD action", () => {
        let msg = getTabDetails("foo", null, { preloaded: true });
        mm.loadedTabs.set(msg.data.browser, msg.data);
        sinon.stub(mm, "onActionFromContent");
        mm.onNewTabLoad({ target: msg.target }, msg.data);
        assert.calledWith(
          mm.onActionFromContent,
          { type: at.NEW_TAB_LOAD },
          "foo"
        );
      });
    });
    describe("#onNewTabUnload", () => {
      it("should dispatch a NEW_TAB_UNLOAD action", () => {
        let msg = getTabDetails("foo");
        mm.loadedTabs.set(msg.data.browser, msg.data);
        sinon.stub(mm, "onActionFromContent");
        mm.onNewTabUnload({ target: msg.target }, msg.data);
        assert.calledWith(
          mm.onActionFromContent,
          { type: at.NEW_TAB_UNLOAD },
          "foo"
        );
      });
    });
    describe("#onMessage", () => {
      let sandbox;
      beforeEach(() => {
        sandbox = sinon.createSandbox();
        sandbox.spy(global.console, "error");
      });
      afterEach(() => sandbox.restore());
      it("return early when tab details are not present", () => {
        let msg = getTabDetails("foo");
        sinon.stub(mm, "onActionFromContent");
        mm.onMessage(msg, msg.data);
        assert.notCalled(mm.onActionFromContent);
      });
      it("should report an error if the msg.data is missing", () => {
        let msg = getTabDetails("foo");
        mm.loadedTabs.set(msg.data.browser, msg.data);
        let tabDetails = msg.data;
        delete msg.data;
        mm.onMessage(msg, tabDetails);
        assert.calledOnce(global.console.error);
      });
      it("should report an error if the msg.data.type is missing", () => {
        let msg = getTabDetails("foo");
        mm.loadedTabs.set(msg.data.browser, msg.data);
        msg.data = "foo";
        mm.onMessage(msg, msg.data);
        assert.calledOnce(global.console.error);
      });
      it("should call onActionFromContent", () => {
        sinon.stub(mm, "onActionFromContent");
        let msg = getTabDetails("foo");
        mm.loadedTabs.set(msg.data.browser, msg.data);
        let action = {
          data: { data: {}, type: "FOO" },
          target: msg.target,
        };
        const expectedAction = {
          type: action.data.type,
          data: action.data.data,
          _target: { browser: msg.data.browser },
        };
        mm.onMessage(action, msg.data);
        assert.calledWith(mm.onActionFromContent, expectedAction, "foo");
      });
    });
  });
  describe("Sending and broadcasting", () => {
    describe("#send", () => {
      it("should send a message on the right port", () => {
        let msg = getTabDetails("foo:6");
        mm.loadedTabs.set(msg.data.browser, msg.data);
        const action = ac.AlsoToOneContent({ type: "HELLO" }, "foo:6");
        mm.send(action);
        assert.calledWith(
          msg.data.actor.sendAsyncMessage,
          DEFAULT_OPTIONS.outgoingMessageName,
          action
        );
      });
      it("should not throw if the target isn't around", () => {
        // port is not added to the channel
        const action = ac.AlsoToOneContent({ type: "HELLO" }, "foo:7");

        assert.doesNotThrow(() => mm.send(action));
      });
    });
    describe("#broadcast", () => {
      it("should send a message on the channel", () => {
        let msg = getTabDetails("foo:8");
        mm.loadedTabs.set(msg.data.browser, msg.data);
        const action = ac.BroadcastToContent({ type: "HELLO" });
        mm.broadcast(action);
        assert.calledWith(
          msg.data.actor.sendAsyncMessage,
          DEFAULT_OPTIONS.outgoingMessageName,
          action
        );
      });
    });
    describe("#preloaded browser", () => {
      it("should send the message to the preloaded browser if there's data and a preloaded browser exists", () => {
        let msg = getTabDetails("foo:9", null, { preloaded: true });
        mm.loadedTabs.set(msg.data.browser, msg.data);
        const action = ac.AlsoToPreloaded({ type: "HELLO", data: 10 });
        mm.sendToPreloaded(action);
        assert.calledWith(
          msg.data.actor.sendAsyncMessage,
          DEFAULT_OPTIONS.outgoingMessageName,
          action
        );
      });
      it("should send the message to all the preloaded browsers if there's data and they exist", () => {
        let msg1 = getTabDetails("foo:10a", null, { preloaded: true });
        mm.loadedTabs.set(msg1.data.browser, msg1.data);

        let msg2 = getTabDetails("foo:10b", null, { preloaded: true });
        mm.loadedTabs.set(msg2.data.browser, msg2.data);

        mm.sendToPreloaded(ac.AlsoToPreloaded({ type: "HELLO", data: 10 }));
        assert.calledOnce(msg1.data.actor.sendAsyncMessage);
        assert.calledOnce(msg2.data.actor.sendAsyncMessage);
      });
      it("should not send the message to the preloaded browser if there's no data and a preloaded browser does not exists", () => {
        let msg = getTabDetails("foo:11");
        mm.loadedTabs.set(msg.data.browser, msg.data);
        const action = ac.AlsoToPreloaded({ type: "HELLO" });
        mm.sendToPreloaded(action);
        assert.notCalled(msg.data.actor.sendAsyncMessage);
      });
    });
  });
  describe("Handling actions", () => {
    describe("#onActionFromContent", () => {
      beforeEach(() => mm.onActionFromContent({ type: "FOO" }, "foo:12"));
      it("should dispatch a AlsoToMain action", () => {
        assert.calledOnce(dispatch);
        const [action] = dispatch.firstCall.args;
        assert.equal(action.type, "FOO", "action.type");
      });
      it("should have the right fromTarget", () => {
        const [action] = dispatch.firstCall.args;
        assert.equal(action.meta.fromTarget, "foo:12", "meta.fromTarget");
      });
    });
    describe("#middleware", () => {
      let store;
      beforeEach(() => {
        store = createStore(addNumberReducer, applyMiddleware(mm.middleware));
      });
      it("should just call next if no channel is found", () => {
        store.dispatch({ type: "ADD", data: 10 });
        assert.equal(store.getState(), 10);
      });
      it("should call .send but not affect the main store if an OnlyToOneContent action is dispatched", () => {
        sinon.stub(mm, "send");
        const action = ac.OnlyToOneContent({ type: "ADD", data: 10 }, "foo");

        store.dispatch(action);

        assert.calledWith(mm.send, action);
        assert.equal(store.getState(), 0);
      });
      it("should call .send and update the main store if an AlsoToOneContent action is dispatched", () => {
        sinon.stub(mm, "send");
        const action = ac.AlsoToOneContent({ type: "ADD", data: 10 }, "foo");

        store.dispatch(action);

        assert.calledWith(mm.send, action);
        assert.equal(store.getState(), 10);
      });
      it("should call .broadcast if the action is BroadcastToContent", () => {
        sinon.stub(mm, "broadcast");
        const action = ac.BroadcastToContent({ type: "FOO" });

        store.dispatch(action);

        assert.calledWith(mm.broadcast, action);
      });
      it("should call .sendToPreloaded if the action is AlsoToPreloaded", () => {
        sinon.stub(mm, "sendToPreloaded");
        const action = ac.AlsoToPreloaded({ type: "FOO" });

        store.dispatch(action);

        assert.calledWith(mm.sendToPreloaded, action);
      });
      it("should dispatch other actions normally", () => {
        sinon.stub(mm, "send");
        sinon.stub(mm, "broadcast");
        sinon.stub(mm, "sendToPreloaded");

        store.dispatch({ type: "ADD", data: 1 });

        assert.equal(store.getState(), 1);
        assert.notCalled(mm.send);
        assert.notCalled(mm.broadcast);
        assert.notCalled(mm.sendToPreloaded);
      });
    });
  });
});
