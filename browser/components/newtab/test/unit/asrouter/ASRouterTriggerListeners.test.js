import {ASRouterTriggerListeners} from "lib/ASRouterTriggerListeners.jsm";

describe("ASRouterTriggerListeners", () => {
  let sandbox;
  let registerWindowNotificationStub;
  let unregisterWindoNotificationStub;
  let windowEnumeratorStub;
  let existingWindow;
  const triggerHandler = () => {};
  const openURLListener = ASRouterTriggerListeners.get("openURL");
  const hosts = ["www.mozilla.com", "www.mozilla.org"];

  function resetEnumeratorStub(windows) {
    windowEnumeratorStub
      .withArgs("navigator:browser")
      .returns(windows);
  }

  beforeEach(async () => {
    sandbox = sinon.sandbox.create();
    registerWindowNotificationStub = sandbox.stub(global.Services.ww, "registerNotification");
    unregisterWindoNotificationStub = sandbox.stub(global.Services.ww, "unregisterNotification");
    existingWindow = {gBrowser: {addTabsProgressListener: sandbox.stub(), removeTabsProgressListener: sandbox.stub()}};
    windowEnumeratorStub = sandbox.stub(global.Services.wm, "getEnumerator");
    resetEnumeratorStub([existingWindow]);
    sandbox.spy(openURLListener, "init");
    sandbox.spy(openURLListener, "uninit");
  });
  afterEach(() => {
    sandbox.restore();
  });

  describe("openURL listener", () => {
    it("should exist and initially be uninitialised", () => {
      assert.ok(openURLListener);
      assert.notOk(openURLListener._initialized);
    });

    describe("#init", () => {
      beforeEach(() => {
        openURLListener.init(triggerHandler, hosts);
      });
      afterEach(() => {
        openURLListener.uninit();
      });

      it("should set ._initialized to true and save the triggerHandler and hosts", () => {
        assert.ok(openURLListener._initialized);
        assert.deepEqual(openURLListener._hosts, new Set(hosts));
        assert.equal(openURLListener._triggerHandler, triggerHandler);
      });

      it("should register an open-window notification", () => {
        assert.calledOnce(registerWindowNotificationStub);
        assert.calledWith(registerWindowNotificationStub, openURLListener);
      });

      it("should add tab progress listeners to all existing browser windows", () => {
        assert.calledOnce(existingWindow.gBrowser.addTabsProgressListener);
        assert.calledWithExactly(existingWindow.gBrowser.addTabsProgressListener, openURLListener);
      });

      it("if already initialised, should only update the trigger handler and add the new hosts", () => {
        const newHosts = ["www.example.com"];
        const newTriggerHandler = () => {};
        resetEnumeratorStub([existingWindow]);
        registerWindowNotificationStub.reset();
        existingWindow.gBrowser.addTabsProgressListener.reset();

        openURLListener.init(newTriggerHandler, newHosts);
        assert.ok(openURLListener._initialized);
        assert.deepEqual(openURLListener._hosts, new Set([...hosts, ...newHosts]));
        assert.equal(openURLListener._triggerHandler, newTriggerHandler);
        assert.notCalled(registerWindowNotificationStub);
        assert.notCalled(existingWindow.gBrowser.addTabsProgressListener);
      });
    });

    describe("#uninit", () => {
      beforeEach(() => {
        openURLListener.init(triggerHandler, hosts);
        // Ensure that the window enumerator will return the existing window for uninit as well
        resetEnumeratorStub([existingWindow]);
        openURLListener.uninit();
      });

      it("should set ._initialized to false and clear the triggerHandler and hosts", () => {
        assert.notOk(openURLListener._initialized);
        assert.equal(openURLListener._hosts, null);
        assert.equal(openURLListener._triggerHandler, null);
      });

      it("should remove an open-window notification", () => {
        assert.calledOnce(unregisterWindoNotificationStub);
        assert.calledWith(unregisterWindoNotificationStub, openURLListener);
      });

      it("should remove tab progress listeners from all existing browser windows", () => {
        assert.calledOnce(existingWindow.gBrowser.removeTabsProgressListener);
        assert.calledWithExactly(existingWindow.gBrowser.removeTabsProgressListener, openURLListener);
      });

      it("should do nothing if already uninitialised", () => {
        unregisterWindoNotificationStub.reset();
        existingWindow.gBrowser.removeTabsProgressListener.reset();
        resetEnumeratorStub([existingWindow]);

        openURLListener.uninit();
        assert.notOk(openURLListener._initialized);
        assert.notCalled(unregisterWindoNotificationStub);
        assert.notCalled(existingWindow.gBrowser.removeTabsProgressListener);
      });
    });

    describe("#onLocationChange", () => {
      it("should call the ._triggerHandler with the right arguments", () => {
        const newTriggerHandler = sinon.stub();
        openURLListener.init(newTriggerHandler, hosts);

        const browser = {};
        const webProgress = {isTopLevel: true};
        const location = "https://www.mozilla.org/something";
        openURLListener.onLocationChange(browser, webProgress, undefined, {spec: location});
        assert.calledOnce(newTriggerHandler);
        assert.calledWithExactly(newTriggerHandler, browser, {id: "openURL", param: "www.mozilla.org"});
      });
    });
  });
});
