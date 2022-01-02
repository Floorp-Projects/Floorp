/*eslint max-nested-callbacks: ["error", 10]*/
import { ASRouterNewTabHook } from "lib/ASRouterNewTabHook.jsm";

describe("ASRouterNewTabHook", () => {
  let sandbox = null;
  let initParams = null;
  beforeEach(() => {
    sandbox = sinon.createSandbox();
    initParams = {
      router: {
        init: sandbox.stub().callsFake(() => {
          // Fake the initialization
          initParams.router.initialized = true;
        }),
        uninit: sandbox.stub(),
      },
      messageHandler: {
        handleCFRAction: {},
        handleTelemetry: {},
      },
      createStorage: () => Promise.resolve({}),
    };
  });
  afterEach(() => {
    sandbox.restore();
  });
  describe("ASRouterNewTabHook", () => {
    describe("getInstance", () => {
      it("awaits createInstance and router init before returning instance", async () => {
        const getInstanceCall = sandbox.spy();
        const waitForInstance = ASRouterNewTabHook.getInstance().then(
          getInstanceCall
        );
        await ASRouterNewTabHook.createInstance(initParams);
        await waitForInstance;
        assert.callOrder(initParams.router.init, getInstanceCall);
      });
    });
    describe("createInstance", () => {
      it("calls router init", async () => {
        await ASRouterNewTabHook.createInstance(initParams);
        assert.calledOnce(initParams.router.init);
      });
      it("only calls router init once", async () => {
        initParams.router.init.callsFake(() => {
          initParams.router.initialized = true;
        });
        await ASRouterNewTabHook.createInstance(initParams);
        await ASRouterNewTabHook.createInstance(initParams);
        assert.calledOnce(initParams.router.init);
      });
    });
    describe("destroy", () => {
      it("disconnects new tab, uninits ASRouter, and destroys instance", async () => {
        await ASRouterNewTabHook.createInstance(initParams);
        const instance = await ASRouterNewTabHook.getInstance();
        const destroy = instance.destroy.bind(instance);
        sandbox.stub(instance, "destroy").callsFake(destroy);
        ASRouterNewTabHook.destroy();
        assert.calledOnce(initParams.router.uninit);
        assert.calledOnce(instance.destroy);
        assert.isNotNull(instance);
        assert.isNull(instance._newTabMessageHandler);
      });
    });
    describe("instance", () => {
      let routerParams = null;
      let messageHandler = null;
      let instance = null;
      beforeEach(async () => {
        messageHandler = {
          clearChildMessages: sandbox.stub().resolves(),
          clearChildProviders: sandbox.stub().resolves(),
          updateAdminState: sandbox.stub().resolves(),
        };
        initParams.router.init.callsFake(params => {
          routerParams = params;
        });
        await ASRouterNewTabHook.createInstance(initParams);
        instance = await ASRouterNewTabHook.getInstance();
      });
      describe("connect", () => {
        it("before connection messageHandler methods are not called", async () => {
          routerParams.clearChildMessages([1]);
          routerParams.clearChildProviders(["snippets"]);
          routerParams.updateAdminState({ messages: {} });
          assert.notCalled(messageHandler.clearChildMessages);
          assert.notCalled(messageHandler.clearChildProviders);
          assert.notCalled(messageHandler.updateAdminState);
        });
        it("after connect updateAdminState and clearChildMessages calls are forwarded to handler", async () => {
          instance.connect(messageHandler);
          routerParams.clearChildMessages([1]);
          routerParams.clearChildProviders(["snippets"]);
          routerParams.updateAdminState({ messages: {} });
          assert.called(messageHandler.clearChildMessages);
          assert.called(messageHandler.clearChildProviders);
          assert.called(messageHandler.updateAdminState);
        });
        it("calls from before connection are dropped", async () => {
          routerParams.clearChildMessages([1]);
          routerParams.clearChildProviders(["snippets"]);
          routerParams.updateAdminState({ messages: {} });
          instance.connect(messageHandler);
          routerParams.clearChildMessages([1]);
          routerParams.clearChildProviders(["snippets"]);
          routerParams.updateAdminState({ messages: {} });
          assert.calledOnce(messageHandler.clearChildMessages);
          assert.calledOnce(messageHandler.clearChildProviders);
          assert.calledOnce(messageHandler.updateAdminState);
        });
      });
      describe("disconnect", () => {
        it("calls after disconnect are dropped", async () => {
          instance.connect(messageHandler);
          instance.disconnect();
          routerParams.clearChildMessages([1]);
          routerParams.clearChildProviders(["snippets"]);
          routerParams.updateAdminState({ messages: {} });
          assert.notCalled(messageHandler.clearChildMessages);
          assert.notCalled(messageHandler.clearChildProviders);
          assert.notCalled(messageHandler.updateAdminState);
        });
        it("only calls from when there is a connection are forwarded", async () => {
          routerParams.clearChildMessages([1]);
          routerParams.clearChildProviders(["foo"]);
          routerParams.updateAdminState({ messages: {} });
          instance.connect(messageHandler);
          routerParams.clearChildMessages([200]);
          routerParams.clearChildProviders(["bar"]);
          routerParams.updateAdminState({
            messages: {
              data: "accept",
            },
          });
          instance.disconnect();
          routerParams.clearChildMessages([1]);
          routerParams.clearChildProviders(["foo"]);
          routerParams.updateAdminState({ messages: {} });
          assert.calledOnce(messageHandler.clearChildMessages);
          assert.calledOnce(messageHandler.clearChildProviders);
          assert.calledOnce(messageHandler.updateAdminState);
          assert.calledWith(messageHandler.clearChildMessages, [200]);
          assert.calledWith(messageHandler.clearChildProviders, ["bar"]);
          assert.calledWith(messageHandler.updateAdminState, {
            messages: {
              data: "accept",
            },
          });
        });
      });
    });
  });
});
