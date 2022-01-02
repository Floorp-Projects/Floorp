import { ASRouterParent } from "actors/ASRouterParent.jsm";
import { MESSAGE_TYPE_HASH as msg } from "common/ActorConstants.jsm";

describe("ASRouterParent", () => {
  let asRouterParent = null;
  let sandbox = null;
  let handleMessage = null;
  let tabs = null;
  beforeEach(() => {
    sandbox = sinon.createSandbox();
    handleMessage = sandbox.stub().resolves("handle-message-result");
    ASRouterParent.nextTabId = 1;
    const methods = {
      destroy: sandbox.stub(),
      size: 1,
      messageAll: sandbox.stub().resolves(),
      registerActor: sandbox.stub(),
      unregisterActor: sandbox.stub(),
      loadingMessageHandler: Promise.resolve({
        handleMessage,
      }),
    };
    tabs = {
      methods,
      factory: sandbox.stub().returns(methods),
    };
    asRouterParent = new ASRouterParent({ tabsFactory: tabs.factory });
    ASRouterParent.tabs = tabs.methods;
    asRouterParent.browsingContext = {
      embedderElement: {
        getAttribute: () => true,
      },
    };
    asRouterParent.tabId = ASRouterParent.nextTabId;
  });
  afterEach(() => {
    sandbox.restore();
    asRouterParent = null;
  });
  describe("actorCreated", () => {
    it("after ASRouterTabs is instanced", () => {
      asRouterParent.actorCreated();
      assert.equal(asRouterParent.tabId, 2);
      assert.notCalled(tabs.factory);
      assert.calledOnce(tabs.methods.registerActor);
    });
    it("before ASRouterTabs is instanced", () => {
      ASRouterParent.tabs = null;
      ASRouterParent.nextTabId = 0;
      asRouterParent.actorCreated();
      assert.calledOnce(tabs.factory);
      assert.isNotNull(ASRouterParent.tabs);
      assert.equal(asRouterParent.tabId, 1);
    });
  });
  describe("didDestroy", () => {
    it("one still remains", () => {
      ASRouterParent.tabs.size = 1;
      asRouterParent.didDestroy();
      assert.isNotNull(ASRouterParent.tabs);
      assert.calledOnce(ASRouterParent.tabs.unregisterActor);
      assert.notCalled(ASRouterParent.tabs.destroy);
    });
    it("none remain", () => {
      ASRouterParent.tabs.size = 0;
      const tabsCopy = ASRouterParent.tabs;
      asRouterParent.didDestroy();
      assert.isNull(ASRouterParent.tabs);
      assert.calledOnce(tabsCopy.unregisterActor);
      assert.calledOnce(tabsCopy.destroy);
    });
  });
  describe("receiveMessage", async () => {
    it("passes call to parentProcessMessageHandler and returns the result from handler", async () => {
      const result = await asRouterParent.receiveMessage({
        name: msg.BLOCK_MESSAGE_BY_ID,
        data: { id: 1 },
      });
      assert.calledOnce(handleMessage);
      assert.equal(result, "handle-message-result");
    });
    it("it messages all actors on BLOCK_MESSAGE_BY_ID messages", async () => {
      const MESSAGE_ID = 1;
      const result = await asRouterParent.receiveMessage({
        name: msg.BLOCK_MESSAGE_BY_ID,
        data: { id: MESSAGE_ID, campaign: "message-campaign" },
      });
      assert.calledOnce(handleMessage);
      // Check that we correctly pass the tabId
      assert.calledWithExactly(
        handleMessage,
        sinon.match.any,
        sinon.match.any,
        { id: sinon.match.number, browser: sinon.match.any }
      );
      assert.calledWithExactly(
        ASRouterParent.tabs.messageAll,
        "ClearMessages",
        // When blocking an id the entire campaign is blocked
        // and all other snippets become invalid
        ["message-campaign"]
      );
      assert.equal(result, "handle-message-result");
    });
  });
});
