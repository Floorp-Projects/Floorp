import { actionCreators as ac, actionTypes as at } from "common/Actions.jsm";
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

describe("ActivityStreamMessageChannel", () => {
  let globals;
  let dispatch;
  let mm;
  let RPmessagePorts;
  beforeEach(() => {
    RPmessagePorts = [];
    function RP(url, isFromAboutNewTab = false) {
      this.url = url;
      this.messagePorts = RPmessagePorts;
      this.addMessageListener = globals.sandbox.spy();
      this.removeMessageListener = globals.sandbox.spy();
      this.sendAsyncMessage = globals.sandbox.spy();
      this.destroy = globals.sandbox.spy();
      this.isFromAboutNewTab = isFromAboutNewTab;
    }
    globals = new GlobalOverrider();
    const overridePageListener = globals.sandbox.stub();
    overridePageListener.withArgs(true).returns(new RP("about:newtab", true));
    overridePageListener.withArgs(false).returns(null);
    globals.set("AboutNewTab", {
      overridePageListener,
      reset: globals.sandbox.spy(),
    });
    globals.set("RemotePages", RP);
    globals.set("AboutHomeStartupCache", { onPreloadedNewTabMessage() {} });
    dispatch = globals.sandbox.spy();
    mm = new ActivityStreamMessageChannel({ dispatch });
  });

  afterEach(() => globals.restore());

  describe("portID validation", () => {
    let sandbox;
    beforeEach(() => {
      sandbox = sinon.createSandbox();
      sandbox.spy(global.Cu, "reportError");
    });
    afterEach(() => {
      sandbox.restore();
    });
    it("should log errors for an invalid portID", () => {
      mm.validatePortID({});
      mm.validatePortID({});
      mm.validatePortID({});

      assert.equal(global.Cu.reportError.callCount, 3);
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
    describe("#createChannel", () => {
      it("should create .channel with the correct URL", () => {
        mm.createChannel();
        assert.ok(mm.channel);
        assert.equal(mm.channel.url, mm.pageURL);
      });
      it("should add 4 message listeners", () => {
        mm.createChannel();
        assert.callCount(mm.channel.addMessageListener, 4);
      });
      it("should add the custom message listener to the channel", () => {
        mm.createChannel();
        assert.calledWith(
          mm.channel.addMessageListener,
          mm.incomingMessageName,
          mm.onMessage
        );
      });
      it("should override AboutNewTab", () => {
        mm.createChannel();
        assert.calledOnce(global.AboutNewTab.overridePageListener);
      });
      it("should use the channel passed by AboutNewTab on override", () => {
        mm.createChannel();
        assert.ok(mm.channel.isFromAboutNewTab);
      });
      it("should not override AboutNewTab if the pageURL is not about:newtab", () => {
        mm = new ActivityStreamMessageChannel({ pageURL: "foo.html" });
        mm.createChannel();
        assert.notCalled(global.AboutNewTab.overridePageListener);
      });
    });
    describe("#simulateMessagesForExistingTabs", () => {
      beforeEach(() => {
        sinon.stub(mm, "onActionFromContent");
        mm.createChannel();
      });
      it("should simulate init for existing ports", () => {
        RPmessagePorts.push({
          url: "about:monkeys",
          loaded: false,
          portID: "inited",
          simulated: true,
          browser: { getAttribute: () => "preloaded" },
        });
        RPmessagePorts.push({
          url: "about:sheep",
          loaded: true,
          portID: "loaded",
          simulated: true,
          browser: { getAttribute: () => "preloaded" },
        });

        mm.simulateMessagesForExistingTabs();

        assert.calledWith(mm.onActionFromContent.firstCall, {
          type: at.NEW_TAB_INIT,
          data: RPmessagePorts[0],
        });
        assert.calledWith(mm.onActionFromContent.secondCall, {
          type: at.NEW_TAB_INIT,
          data: RPmessagePorts[1],
        });
      });
      it("should simulate load for loaded ports", () => {
        RPmessagePorts.push({
          loaded: true,
          portID: "foo",
          browser: { getAttribute: () => "preloaded" },
        });

        mm.simulateMessagesForExistingTabs();

        assert.calledWith(
          mm.onActionFromContent,
          { type: at.NEW_TAB_LOAD },
          "foo"
        );
      });
      it("should set renderLayers on preloaded browsers after load", () => {
        RPmessagePorts.push({
          loaded: true,
          portID: "foo",
          browser: { getAttribute: () => "preloaded" },
        });
        mm.simulateMessagesForExistingTabs();
        assert.equal(RPmessagePorts[0].browser.renderLayers, true);
      });
    });
    describe("#destroyChannel", () => {
      let channel;
      beforeEach(() => {
        mm.createChannel();
        channel = mm.channel;
      });
      it("should set .channel to null", () => {
        mm.destroyChannel();
        assert.isNull(mm.channel);
      });
      it("should reset AboutNewTab, and pass back its channel", () => {
        mm.destroyChannel();
        assert.calledOnce(global.AboutNewTab.reset);
        assert.calledWith(global.AboutNewTab.reset, channel);
      });
      it("should not reset AboutNewTab if the pageURL is not about:newtab", () => {
        mm = new ActivityStreamMessageChannel({ pageURL: "foo.html" });
        mm.createChannel();
        mm.destroyChannel();
        assert.notCalled(global.AboutNewTab.reset);
      });
      it("should call channel.destroy() if pageURL is not about:newtab", () => {
        mm = new ActivityStreamMessageChannel({ pageURL: "foo.html" });
        mm.createChannel();
        channel = mm.channel;
        mm.destroyChannel();
        assert.calledOnce(channel.destroy);
      });
    });
  });
  describe("Message handling", () => {
    describe("#getTargetById", () => {
      it("should get an id if it exists", () => {
        const t = { portID: "foo:1" };
        mm.createChannel();
        mm.channel.messagePorts.push(t);
        assert.equal(mm.getTargetById("foo:1"), t);
      });
      it("should return null if the target doesn't exist", () => {
        const t = { portID: "foo:2" };
        mm.createChannel();
        mm.channel.messagePorts.push(t);
        assert.equal(mm.getTargetById("bar:3"), null);
      });
    });
    describe("#getPreloadedBrowser", () => {
      it("should get a preloaded browser if it exists", () => {
        const port = {
          browser: {
            getAttribute() {
              return "preloaded";
            },
          },
        };
        mm.createChannel();
        mm.channel.messagePorts.push(port);
        assert.equal(mm.getPreloadedBrowser()[0], port);
      });
      it("should get all the preloaded browsers across windows if they exist", () => {
        const port = {
          browser: {
            getAttribute() {
              return "preloaded";
            },
          },
        };
        mm.createChannel();
        mm.channel.messagePorts.push(port);
        mm.channel.messagePorts.push(port);
        assert.equal(mm.getPreloadedBrowser().length, 2);
      });
      it("should return null if there is no preloaded browser", () => {
        const port = {
          browser: {
            getAttribute() {
              return "consumed";
            },
          },
        };
        mm.createChannel();
        mm.channel.messagePorts.push(port);
        assert.equal(mm.getPreloadedBrowser(), null);
      });
    });
    describe("#onNewTabInit", () => {
      it("should dispatch a NEW_TAB_INIT action", () => {
        const t = { portID: "foo", url: "about:monkeys" };
        sinon.stub(mm, "onActionFromContent");

        mm.onNewTabInit({ target: t });

        assert.calledWith(mm.onActionFromContent, {
          type: at.NEW_TAB_INIT,
          data: t,
        });
      });
    });
    describe("#onNewTabLoad", () => {
      it("should dispatch a NEW_TAB_LOAD action", () => {
        const t = {
          portID: "foo",
          browser: { getAttribute: () => "preloaded" },
        };
        sinon.stub(mm, "onActionFromContent");
        mm.onNewTabLoad({ target: t });
        assert.calledWith(
          mm.onActionFromContent,
          { type: at.NEW_TAB_LOAD },
          "foo"
        );
      });
    });
    describe("#onNewTabUnload", () => {
      it("should dispatch a NEW_TAB_UNLOAD action", () => {
        const t = { portID: "foo" };
        sinon.stub(mm, "onActionFromContent");
        mm.onNewTabUnload({ target: t });
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
        sandbox.spy(global.Cu, "reportError");
      });
      afterEach(() => sandbox.restore());
      it("should report an error if the msg.data is missing", () => {
        mm.onMessage({ target: { portID: "foo" } });
        assert.calledOnce(global.Cu.reportError);
      });
      it("should report an error if the msg.data.type is missing", () => {
        mm.onMessage({ target: { portID: "foo" }, data: "foo" });
        assert.calledOnce(global.Cu.reportError);
      });
      it("should call onActionFromContent", () => {
        sinon.stub(mm, "onActionFromContent");
        const action = {
          data: { data: {}, type: "FOO" },
          target: { portID: "foo" },
        };
        const expectedAction = {
          type: action.data.type,
          data: action.data.data,
          _target: { portID: "foo" },
        };
        mm.onMessage(action);
        assert.calledWith(mm.onActionFromContent, expectedAction, "foo");
      });
    });
  });
  describe("Sending and broadcasting", () => {
    describe("#send", () => {
      it("should send a message on the right port", () => {
        const t = { portID: "foo:3", sendAsyncMessage: sinon.spy() };
        mm.createChannel();
        mm.channel.messagePorts = [t];
        const action = ac.AlsoToOneContent({ type: "HELLO" }, "foo:3");
        mm.send(action);
        assert.calledWith(
          t.sendAsyncMessage,
          DEFAULT_OPTIONS.outgoingMessageName,
          action
        );
      });
      it("should not throw if the target isn't around", () => {
        mm.createChannel();
        // port is not added to the channel
        const action = ac.AlsoToOneContent({ type: "HELLO" }, "foo:4");

        assert.doesNotThrow(() => mm.send(action));
      });
    });
    describe("#broadcast", () => {
      it("should send a message on the channel", () => {
        mm.createChannel();
        const action = ac.BroadcastToContent({ type: "HELLO" });
        mm.broadcast(action);
        assert.calledWith(
          mm.channel.sendAsyncMessage,
          DEFAULT_OPTIONS.outgoingMessageName,
          action
        );
      });
    });
    describe("#preloaded browser", () => {
      it("should send the message to the preloaded browser if there's data and a preloaded browser exists", () => {
        const port = {
          browser: {
            getAttribute() {
              return "preloaded";
            },
          },
          sendAsyncMessage: sinon.spy(),
        };
        mm.createChannel();
        mm.channel.messagePorts.push(port);
        const action = ac.AlsoToPreloaded({ type: "HELLO", data: 10 });
        mm.sendToPreloaded(action);
        assert.calledWith(
          port.sendAsyncMessage,
          DEFAULT_OPTIONS.outgoingMessageName,
          action
        );
      });
      it("should send the message to all the preloaded browsers if there's data and they exist", () => {
        const port = {
          browser: {
            getAttribute() {
              return "preloaded";
            },
          },
          sendAsyncMessage: sinon.spy(),
        };
        mm.createChannel();
        mm.channel.messagePorts.push(port);
        mm.channel.messagePorts.push(port);
        mm.sendToPreloaded(ac.AlsoToPreloaded({ type: "HELLO", data: 10 }));
        assert.calledTwice(port.sendAsyncMessage);
      });
      it("should not send the message to the preloaded browser if there's no data and a preloaded browser does not exists", () => {
        const port = {
          browser: {
            getAttribute() {
              return "consumed";
            },
          },
          sendAsyncMessage: sinon.spy(),
        };
        mm.createChannel();
        mm.channel.messagePorts.push(port);
        const action = ac.AlsoToPreloaded({ type: "HELLO" });
        mm.sendToPreloaded(action);
        assert.notCalled(port.sendAsyncMessage);
      });
    });
  });
  describe("Handling actions", () => {
    describe("#onActionFromContent", () => {
      beforeEach(() => mm.onActionFromContent({ type: "FOO" }, "foo:5"));
      it("should dispatch a AlsoToMain action", () => {
        assert.calledOnce(dispatch);
        const [action] = dispatch.firstCall.args;
        assert.equal(action.type, "FOO", "action.type");
      });
      it("should have the right fromTarget", () => {
        const [action] = dispatch.firstCall.args;
        assert.equal(action.meta.fromTarget, "foo:5", "meta.fromTarget");
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
        mm.createChannel();

        store.dispatch(action);

        assert.calledWith(mm.send, action);
        assert.equal(store.getState(), 0);
      });
      it("should call .send and update the main store if an AlsoToOneContent action is dispatched", () => {
        sinon.stub(mm, "send");
        const action = ac.AlsoToOneContent({ type: "ADD", data: 10 }, "foo");
        mm.createChannel();

        store.dispatch(action);

        assert.calledWith(mm.send, action);
        assert.equal(store.getState(), 10);
      });
      it("should call .broadcast if the action is BroadcastToContent", () => {
        sinon.stub(mm, "broadcast");
        const action = ac.BroadcastToContent({ type: "FOO" });

        mm.createChannel();
        store.dispatch(action);

        assert.calledWith(mm.broadcast, action);
      });
      it("should call .sendToPreloaded if the action is AlsoToPreloaded", () => {
        sinon.stub(mm, "sendToPreloaded");
        const action = ac.AlsoToPreloaded({ type: "FOO" });

        mm.createChannel();
        store.dispatch(action);

        assert.calledWith(mm.sendToPreloaded, action);
      });
      it("should dispatch other actions normally", () => {
        sinon.stub(mm, "send");
        sinon.stub(mm, "broadcast");
        sinon.stub(mm, "sendToPreloaded");

        mm.createChannel();
        store.dispatch({ type: "ADD", data: 1 });

        assert.equal(store.getState(), 1);
        assert.notCalled(mm.send);
        assert.notCalled(mm.broadcast);
        assert.notCalled(mm.sendToPreloaded);
      });
    });
  });
});
