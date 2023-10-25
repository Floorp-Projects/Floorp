import { ASRouterParentProcessMessageHandler } from "lib/ASRouterParentProcessMessageHandler.jsm";
import { _ASRouter } from "lib/ASRouter.jsm";
import { MESSAGE_TYPE_HASH as msg } from "common/ActorConstants.sys.mjs";

describe("ASRouterParentProcessMessageHandler", () => {
  let handler = null;
  let sandbox = null;
  let config = null;
  beforeEach(() => {
    sandbox = sinon.createSandbox();
    const returnValue = { value: 1 };
    const router = new _ASRouter();
    [
      "addImpression",
      "addPreviewEndpoint",
      "evaluateExpression",
      "forceAttribution",
      "forceWNPanel",
      "closeWNPanel",
      "forcePBWindow",
      "resetGroupsState",
      "resetMessageState",
      "resetScreenImpressions",
      "editState",
    ].forEach(method => sandbox.stub(router, `${method}`).resolves());
    [
      "blockMessageById",
      "loadMessagesFromAllProviders",
      "sendNewTabMessage",
      "sendTriggerMessage",
      "routeCFRMessage",
      "setMessageById",
      "updateTargetingParameters",
      "unblockMessageById",
      "unblockAll",
    ].forEach(method =>
      sandbox.stub(router, `${method}`).resolves(returnValue)
    );
    router._storage = {
      set: sandbox.stub().resolves(),
      get: sandbox.stub().resolves(),
    };
    sandbox.stub(router, "setState").callsFake(callback => {
      if (typeof callback === "function") {
        callback({
          messageBlockList: [
            {
              id: 0,
            },
            {
              id: 1,
            },
            {
              id: 2,
            },
            {
              id: 3,
            },
            {
              id: 4,
            },
          ],
        });
      }
      return Promise.resolve(returnValue);
    });
    const preferences = {
      enableOrDisableProvider: sandbox.stub(),
      resetProviderPref: sandbox.stub(),
      setUserPreference: sandbox.stub(),
    };
    const specialMessageActions = {
      handleAction: sandbox.stub(),
    };
    const queryCache = {
      expireAll: sandbox.stub(),
    };
    const sendTelemetry = sandbox.stub();
    config = {
      router,
      preferences,
      specialMessageActions,
      queryCache,
      sendTelemetry,
    };
    handler = new ASRouterParentProcessMessageHandler(config);
  });
  afterEach(() => {
    sandbox.restore();
    handler = null;
    config = null;
  });
  describe("constructor", () => {
    it("does not throw", () => {
      assert.isNotNull(handler);
      assert.isNotNull(config);
    });
  });
  describe("handleCFRAction", () => {
    it("non-telemetry type isn't sent to telemetry", () => {
      handler.handleCFRAction({
        type: msg.BLOCK_MESSAGE_BY_ID,
        data: { id: 1 },
      });
      assert.notCalled(config.sendTelemetry);
      assert.calledOnce(config.router.blockMessageById);
    });
    it("passes browser to handleMessage", async () => {
      await handler.handleCFRAction(
        {
          type: msg.USER_ACTION,
          data: { id: 1 },
        },
        { ownerGlobal: {} }
      );
      assert.notCalled(config.sendTelemetry);
      assert.calledOnce(config.specialMessageActions.handleAction);
      assert.calledWith(
        config.specialMessageActions.handleAction,
        { id: 1 },
        { ownerGlobal: {} }
      );
    });
    [
      msg.AS_ROUTER_TELEMETRY_USER_EVENT,
      msg.TOOLBAR_BADGE_TELEMETRY,
      msg.TOOLBAR_PANEL_TELEMETRY,
      msg.MOMENTS_PAGE_TELEMETRY,
      msg.DOORHANGER_TELEMETRY,
    ].forEach(type => {
      it(`telemetry type "${type}" is sent to telemetry`, () => {
        handler.handleCFRAction({
          type,
          data: { id: 1 },
        });
        assert.calledOnce(config.sendTelemetry);
        assert.notCalled(config.router.blockMessageById);
      });
    });
  });
  describe("#handleMessage", () => {
    it("#default: should throw for unknown msg types", () => {
      handler.handleMessage("err").then(
        () => assert.fail("It should not succeed"),
        () => assert.ok(true)
      );
    });
    describe("#AS_ROUTER_TELEMETRY_USER_EVENT", () => {
      it("should route AS_ROUTER_TELEMETRY_USER_EVENT to handleTelemetry", async () => {
        const data = { data: "foo" };
        await handler.handleMessage(msg.AS_ROUTER_TELEMETRY_USER_EVENT, data);

        assert.calledOnce(handler.handleTelemetry);
        assert.calledWithExactly(handler.handleTelemetry, {
          type: msg.AS_ROUTER_TELEMETRY_USER_EVENT,
          data,
        });
      });
    });
    describe("BLOCK_MESSAGE_BY_ID action", () => {
      it("with preventDismiss returns false", async () => {
        const result = await handler.handleMessage(msg.BLOCK_MESSAGE_BY_ID, {
          id: 1,
          preventDismiss: true,
        });
        assert.calledOnce(config.router.blockMessageById);
        assert.isFalse(result);
      });
      it("by default returns true", async () => {
        const result = await handler.handleMessage(msg.BLOCK_MESSAGE_BY_ID, {
          id: 1,
        });
        assert.calledOnce(config.router.blockMessageById);
        assert.isTrue(result);
      });
    });
    describe("USER_ACTION action", () => {
      it("default calls SpecialMessageActions.handleAction", async () => {
        await handler.handleMessage(
          msg.USER_ACTION,
          {
            type: "SOMETHING",
          },
          { browser: { ownerGlobal: {} } }
        );
        assert.calledOnce(config.specialMessageActions.handleAction);
        assert.calledWith(
          config.specialMessageActions.handleAction,
          { type: "SOMETHING" },
          { ownerGlobal: {} }
        );
      });
    });
    describe("IMPRESSION action", () => {
      it("default calls addImpression", () => {
        handler.handleMessage(msg.IMPRESSION, {
          id: 1,
        });
        assert.calledOnce(config.router.addImpression);
      });
    });
    describe("TRIGGER action", () => {
      it("default calls sendTriggerMessage and returns state", async () => {
        const result = await handler.handleMessage(
          msg.TRIGGER,
          {
            trigger: { stuff: {} },
          },
          { id: 100, browser: { ownerGlobal: {} } }
        );
        assert.calledOnce(config.router.sendTriggerMessage);
        assert.calledWith(config.router.sendTriggerMessage, {
          stuff: {},
          tabId: 100,
          browser: { ownerGlobal: {} },
        });
        assert.deepEqual(result, { value: 1 });
      });
    });
    describe("NEWTAB_MESSAGE_REQUEST action", () => {
      it("default calls sendNewTabMessage and returns state", async () => {
        const result = await handler.handleMessage(
          msg.NEWTAB_MESSAGE_REQUEST,
          {
            stuff: {},
          },
          { id: 100, browser: { ownerGlobal: {} } }
        );
        assert.calledOnce(config.router.sendNewTabMessage);
        assert.calledWith(config.router.sendNewTabMessage, {
          stuff: {},
          tabId: 100,
          browser: { ownerGlobal: {} },
        });
        assert.deepEqual(result, { value: 1 });
      });
    });
    describe("ADMIN_CONNECT_STATE action", () => {
      it("with endpoint url calls addPreviewEndpoint, loadMessagesFromAllProviders, and returns state", async () => {
        const result = await handler.handleMessage(msg.ADMIN_CONNECT_STATE, {
          endpoint: {
            url: "test",
          },
        });
        assert.calledOnce(config.router.addPreviewEndpoint);
        assert.calledOnce(config.router.loadMessagesFromAllProviders);
        assert.deepEqual(result, { value: 1 });
      });
      it("default returns state", async () => {
        const result = await handler.handleMessage(msg.ADMIN_CONNECT_STATE);
        assert.calledOnce(config.router.updateTargetingParameters);
        assert.deepEqual(result, { value: 1 });
      });
    });
    describe("UNBLOCK_MESSAGE_BY_ID action", () => {
      it("default calls unblockMessageById", async () => {
        const result = await handler.handleMessage(msg.UNBLOCK_MESSAGE_BY_ID, {
          id: 1,
        });
        assert.calledOnce(config.router.unblockMessageById);
        assert.deepEqual(result, { value: 1 });
      });
    });
    describe("UNBLOCK_ALL action", () => {
      it("default calls unblockAll", async () => {
        const result = await handler.handleMessage(msg.UNBLOCK_ALL);
        assert.calledOnce(config.router.unblockAll);
        assert.deepEqual(result, { value: 1 });
      });
    });
    describe("BLOCK_BUNDLE action", () => {
      it("default calls unblockMessageById", async () => {
        const result = await handler.handleMessage(msg.BLOCK_BUNDLE, {
          bundle: [
            {
              id: 8,
            },
            {
              id: 13,
            },
          ],
        });
        assert.calledOnce(config.router.blockMessageById);
        assert.deepEqual(result, { value: 1 });
      });
    });
    describe("UNBLOCK_BUNDLE action", () => {
      it("default calls setState", async () => {
        const result = await handler.handleMessage(msg.UNBLOCK_BUNDLE, {
          bundle: [
            {
              id: 1,
            },
            {
              id: 3,
            },
          ],
        });
        assert.calledOnce(config.router.setState);
        assert.deepEqual(result, { value: 1 });
      });
    });
    describe("DISABLE_PROVIDER action", () => {
      it("default calls ASRouterPreferences.enableOrDisableProvider", () => {
        handler.handleMessage(msg.DISABLE_PROVIDER, {});
        assert.calledOnce(config.preferences.enableOrDisableProvider);
      });
    });
    describe("ENABLE_PROVIDER action", () => {
      it("default calls ASRouterPreferences.enableOrDisableProvider", () => {
        handler.handleMessage(msg.ENABLE_PROVIDER, {});
        assert.calledOnce(config.preferences.enableOrDisableProvider);
      });
    });
    describe("EVALUATE_JEXL_EXPRESSION action", () => {
      it("default calls evaluateExpression", () => {
        handler.handleMessage(msg.EVALUATE_JEXL_EXPRESSION, {});
        assert.calledOnce(config.router.evaluateExpression);
      });
    });
    describe("EXPIRE_QUERY_CACHE action", () => {
      it("default calls QueryCache.expireAll", () => {
        handler.handleMessage(msg.EXPIRE_QUERY_CACHE);
        assert.calledOnce(config.queryCache.expireAll);
      });
    });
    describe("FORCE_ATTRIBUTION action", () => {
      it("default calls forceAttribution", () => {
        handler.handleMessage(msg.FORCE_ATTRIBUTION, {});
        assert.calledOnce(config.router.forceAttribution);
      });
    });
    describe("FORCE_WHATSNEW_PANEL action", () => {
      it("default calls forceWNPanel", () => {
        handler.handleMessage(
          msg.FORCE_WHATSNEW_PANEL,
          {},
          { browser: { ownerGlobal: {} } }
        );
        assert.calledOnce(config.router.forceWNPanel);
        assert.calledWith(config.router.forceWNPanel, { ownerGlobal: {} });
      });
    });
    describe("CLOSE_WHATSNEW_PANEL action", () => {
      it("default calls closeWNPanel", () => {
        handler.handleMessage(
          msg.CLOSE_WHATSNEW_PANEL,
          {},
          { browser: { ownerGlobal: {} } }
        );
        assert.calledOnce(config.router.closeWNPanel);
        assert.calledWith(config.router.closeWNPanel, { ownerGlobal: {} });
      });
    });
    describe("FORCE_PRIVATE_BROWSING_WINDOW action", () => {
      it("default calls forcePBWindow", () => {
        handler.handleMessage(
          msg.FORCE_PRIVATE_BROWSING_WINDOW,
          {},
          { browser: { ownerGlobal: {} } }
        );
        assert.calledOnce(config.router.forcePBWindow);
        assert.calledWith(config.router.forcePBWindow, { ownerGlobal: {} });
      });
    });
    describe("MODIFY_MESSAGE_JSON action", () => {
      it("default calls routeCFRMessage", async () => {
        const result = await handler.handleMessage(
          msg.MODIFY_MESSAGE_JSON,
          {
            content: {
              text: "something",
            },
          },
          { browser: { ownerGlobal: {} }, id: 100 }
        );
        assert.calledOnce(config.router.routeCFRMessage);
        assert.calledWith(
          config.router.routeCFRMessage,
          { text: "something" },
          { ownerGlobal: {} },
          { content: { text: "something" } },
          true
        );
        assert.deepEqual(result, { value: 1 });
      });
    });
    describe("OVERRIDE_MESSAGE action", () => {
      it("default calls setMessageById", async () => {
        const result = await handler.handleMessage(
          msg.OVERRIDE_MESSAGE,
          {
            id: 1,
          },
          { id: 100, browser: { ownerGlobal: {} } }
        );
        assert.calledOnce(config.router.setMessageById);
        assert.calledWith(config.router.setMessageById, { id: 1 }, true, {
          ownerGlobal: {},
        });
        assert.deepEqual(result, { value: 1 });
      });
    });
    describe("RESET_PROVIDER_PREF action", () => {
      it("default calls ASRouterPreferences.resetProviderPref", () => {
        handler.handleMessage(msg.RESET_PROVIDER_PREF);
        assert.calledOnce(config.preferences.resetProviderPref);
      });
    });
    describe("SET_PROVIDER_USER_PREF action", () => {
      it("default calls ASRouterPreferences.setUserPreference", () => {
        handler.handleMessage(msg.SET_PROVIDER_USER_PREF, {
          id: 1,
          value: true,
        });
        assert.calledOnce(config.preferences.setUserPreference);
        assert.calledWith(config.preferences.setUserPreference, 1, true);
      });
    });
    describe("RESET_GROUPS_STATE action", () => {
      it("default calls resetGroupsState, loadMessagesFromAllProviders, and returns state", async () => {
        const result = await handler.handleMessage(msg.RESET_GROUPS_STATE, {
          property: "value",
        });
        assert.calledOnce(config.router.resetGroupsState);
        assert.calledOnce(config.router.loadMessagesFromAllProviders);
        assert.deepEqual(result, { value: 1 });
      });
    });
    describe("RESET_MESSAGE_STATE action", () => {
      it("default calls resetMessageState", () => {
        handler.handleMessage(msg.RESET_MESSAGE_STATE);
        assert.calledOnce(config.router.resetMessageState);
      });
    });
    describe("RESET_SCREEN_IMPRESSIONS action", () => {
      it("default calls resetScreenImpressions", () => {
        handler.handleMessage(msg.RESET_SCREEN_IMPRESSIONS);
        assert.calledOnce(config.router.resetScreenImpressions);
      });
    });
    describe("EDIT_STATE action", () => {
      it("default calls editState with correct args", () => {
        handler.handleMessage(msg.EDIT_STATE, { property: "value" });
        assert.calledWith(config.router.editState, "property", "value");
      });
    });
  });
});
