import { ASRouterTriggerListeners } from "lib/ASRouterTriggerListeners.jsm";
import { ASRouterPreferences } from "lib/ASRouterPreferences.jsm";
import { GlobalOverrider } from "test/unit/utils";

describe("ASRouterTriggerListeners", () => {
  let sandbox;
  let globals;
  let existingWindow;
  let isWindowPrivateStub;
  const triggerHandler = () => {};
  const openURLListener = ASRouterTriggerListeners.get("openURL");
  const frequentVisitsListener = ASRouterTriggerListeners.get("frequentVisits");
  const captivePortalLoginListener =
    ASRouterTriggerListeners.get("captivePortalLogin");
  const bookmarkedURLListener =
    ASRouterTriggerListeners.get("openBookmarkedURL");
  const openArticleURLListener = ASRouterTriggerListeners.get("openArticleURL");
  const nthTabClosedListener = ASRouterTriggerListeners.get("nthTabClosed");
  const idleListener = ASRouterTriggerListeners.get("activityAfterIdle");
  const formAutofillListener = ASRouterTriggerListeners.get("formAutofill");
  const cookieBannerDetectedListener = ASRouterTriggerListeners.get(
    "cookieBannerDetected"
  );
  const cookieBannerHandledListener = ASRouterTriggerListeners.get(
    "cookieBannerHandled"
  );
  const hosts = ["www.mozilla.com", "www.mozilla.org"];

  const regionFake = {
    _home: "cn",
    _current: "cn",
    get home() {
      return this._home;
    },
    get current() {
      return this._current;
    },
  };

  beforeEach(async () => {
    sandbox = sinon.createSandbox();
    globals = new GlobalOverrider();
    existingWindow = {
      gBrowser: {
        addTabsProgressListener: sandbox.stub(),
        removeTabsProgressListener: sandbox.stub(),
        currentURI: { host: "" },
      },
      addEventListener: sinon.stub(),
      removeEventListener: sinon.stub(),
    };
    sandbox.spy(openURLListener, "init");
    sandbox.spy(openURLListener, "uninit");
    isWindowPrivateStub = sandbox.stub();
    // Assume no window is private so that we execute the action
    isWindowPrivateStub.returns(false);
    globals.set("PrivateBrowsingUtils", {
      isWindowPrivate: isWindowPrivateStub,
    });
    const ewUninit = new Map();
    globals.set("EveryWindow", {
      registerCallback: (id, init, uninit) => {
        init(existingWindow);
        ewUninit.set(id, uninit);
      },
      unregisterCallback: id => {
        ewUninit.get(id)(existingWindow);
      },
    });
    globals.set("Region", regionFake);
    globals.set("ASRouterPreferences", ASRouterPreferences);
  });

  afterEach(() => {
    sandbox.restore();
    globals.restore();
  });

  describe("openBookmarkedURL", () => {
    let observerStub;
    describe("#init", () => {
      beforeEach(() => {
        observerStub = sandbox.stub(global.Services.obs, "addObserver");
        sandbox
          .stub(global.Services.wm, "getMostRecentBrowserWindow")
          .returns({ gBrowser: { selectedBrowser: {} } });
      });
      afterEach(() => {
        bookmarkedURLListener.uninit();
      });
      it("should set hosts to the recentBookmarks", async () => {
        await bookmarkedURLListener.init(sandbox.stub());

        assert.calledOnce(observerStub);
        assert.calledWithExactly(
          observerStub,
          bookmarkedURLListener,
          "bookmark-icon-updated"
        );
      });
      it("should provide id to triggerHandler", async () => {
        const newTriggerHandler = sinon.stub();
        const subject = {};
        await bookmarkedURLListener.init(newTriggerHandler);

        bookmarkedURLListener.observe(
          subject,
          "bookmark-icon-updated",
          "starred"
        );

        assert.calledOnce(newTriggerHandler);
        assert.calledWithExactly(newTriggerHandler, subject, {
          id: bookmarkedURLListener.id,
        });
      });
    });
  });

  describe("captivePortal", () => {
    describe("observe", () => {
      it("should not call the trigger handler if _shouldShowCaptivePortalVPNPromo returns false", () => {
        sandbox
          .stub(captivePortalLoginListener, "_shouldShowCaptivePortalVPNPromo")
          .returns(false);
        captivePortalLoginListener._triggerHandler = sandbox.spy();

        captivePortalLoginListener.observe(
          null,
          "captive-portal-login-success"
        );

        assert.notCalled(captivePortalLoginListener._triggerHandler);
      });

      it("should call the trigger handler if _shouldShowCaptivePortalVPNPromo returns true", () => {
        sandbox
          .stub(captivePortalLoginListener, "_shouldShowCaptivePortalVPNPromo")
          .returns(true);
        sandbox.stub(Services.wm, "getMostRecentBrowserWindow").returns({
          gBrowser: {
            selectedBrowser: true,
          },
        });
        captivePortalLoginListener._triggerHandler = sandbox.spy();

        captivePortalLoginListener.observe(
          null,
          "captive-portal-login-success"
        );

        assert.calledOnce(captivePortalLoginListener._triggerHandler);
      });
    });
  });

  describe("openArticleURL", () => {
    describe("#init", () => {
      beforeEach(() => {
        globals.set(
          "MatchPatternSet",
          sandbox.stub().callsFake(patterns => ({
            patterns,
            matches: url => patterns.has(url),
          }))
        );
        sandbox.stub(global.AboutReaderParent, "addMessageListener");
        sandbox.stub(global.AboutReaderParent, "removeMessageListener");
      });
      afterEach(() => {
        openArticleURLListener.uninit();
      });
      it("setup an event listener on init", () => {
        openArticleURLListener.init(sandbox.stub(), hosts, hosts);

        assert.calledOnce(global.AboutReaderParent.addMessageListener);
        assert.calledWithExactly(
          global.AboutReaderParent.addMessageListener,
          openArticleURLListener.readerModeEvent,
          sinon.match.object
        );
      });
      it("should call triggerHandler correctly for matches [host match]", () => {
        const stub = sandbox.stub();
        const target = { currentURI: { host: hosts[0], spec: hosts[1] } };
        openArticleURLListener.init(stub, hosts, hosts);

        const [, { receiveMessage }] =
          global.AboutReaderParent.addMessageListener.firstCall.args;
        receiveMessage({ data: { isArticle: true }, target });

        assert.calledOnce(stub);
        assert.calledWithExactly(stub, target, {
          id: openArticleURLListener.id,
          param: { host: hosts[0], url: hosts[1] },
        });
      });
      it("should call triggerHandler correctly for matches [pattern match]", () => {
        const stub = sandbox.stub();
        const target = { currentURI: { host: null, spec: hosts[1] } };
        openArticleURLListener.init(stub, hosts, hosts);

        const [, { receiveMessage }] =
          global.AboutReaderParent.addMessageListener.firstCall.args;
        receiveMessage({ data: { isArticle: true }, target });

        assert.calledOnce(stub);
        assert.calledWithExactly(stub, target, {
          id: openArticleURLListener.id,
          param: { host: null, url: hosts[1] },
        });
      });
      it("should remove the message listener", () => {
        openArticleURLListener.init(sandbox.stub(), hosts, hosts);
        openArticleURLListener.uninit();

        assert.calledOnce(global.AboutReaderParent.removeMessageListener);
      });
    });
  });

  describe("frequentVisits", () => {
    let _triggerHandler;
    beforeEach(() => {
      _triggerHandler = sandbox.stub();
      sandbox.useFakeTimers();
      frequentVisitsListener.init(_triggerHandler, hosts);
    });
    afterEach(() => {
      sandbox.clock.restore();
      frequentVisitsListener.uninit();
    });
    it("should be initialized", () => {
      assert.isTrue(frequentVisitsListener._initialized);
    });
    it("should listen for TabSelect events", () => {
      assert.calledOnce(existingWindow.addEventListener);
      assert.calledWith(
        existingWindow.addEventListener,
        "TabSelect",
        frequentVisitsListener.onTabSwitch
      );
    });
    it("should call _triggerHandler if the visit is valid (is recoreded)", () => {
      frequentVisitsListener.triggerHandler({}, "www.mozilla.com");

      assert.calledOnce(_triggerHandler);
    });
    it("should call _triggerHandler only once", () => {
      frequentVisitsListener.triggerHandler({}, "www.mozilla.com");
      frequentVisitsListener.triggerHandler({}, "www.mozilla.com");

      assert.calledOnce(_triggerHandler);
    });
    it("should call _triggerHandler again after 15 minutes", () => {
      frequentVisitsListener.triggerHandler({}, "www.mozilla.com");
      sandbox.clock.tick(15 * 60 * 1000 + 1);
      frequentVisitsListener.triggerHandler({}, "www.mozilla.com");

      assert.calledTwice(_triggerHandler);
    });
    it("should call triggerHandler on valid hosts", () => {
      const stub = sandbox.stub(frequentVisitsListener, "triggerHandler");
      existingWindow.gBrowser.currentURI.host = hosts[0]; // eslint-disable-line prefer-destructuring

      frequentVisitsListener.onTabSwitch({
        target: { ownerGlobal: existingWindow },
      });

      assert.calledOnce(stub);
    });
    it("should not call triggerHandler on invalid hosts", () => {
      const stub = sandbox.stub(frequentVisitsListener, "triggerHandler");
      existingWindow.gBrowser.currentURI.host = "foo.com";

      frequentVisitsListener.onTabSwitch({
        target: { ownerGlobal: existingWindow },
      });

      assert.notCalled(stub);
    });
    describe("MatchPattern", () => {
      beforeEach(() => {
        globals.set(
          "MatchPatternSet",
          sandbox.stub().callsFake(patterns => ({ patterns: patterns || [] }))
        );
      });
      afterEach(() => {
        frequentVisitsListener.uninit();
      });
      it("should create a matchPatternSet", () => {
        frequentVisitsListener.init(_triggerHandler, hosts, ["pattern"]);

        assert.calledOnce(window.MatchPatternSet);
        assert.calledWithExactly(
          window.MatchPatternSet,
          new Set(["pattern"]),
          undefined
        );
      });
      it("should allow to add multiple patterns and dedupe", () => {
        frequentVisitsListener.init(_triggerHandler, hosts, ["pattern"]);
        frequentVisitsListener.init(_triggerHandler, hosts, ["foo"]);

        assert.calledTwice(window.MatchPatternSet);
        assert.calledWithExactly(
          window.MatchPatternSet,
          new Set(["pattern", "foo"]),
          undefined
        );
      });
      it("should handle bad arguments to MatchPatternSet", () => {
        const badArgs = ["www.example.com"];
        window.MatchPatternSet.withArgs(new Set(badArgs)).throws();
        frequentVisitsListener.init(_triggerHandler, hosts, badArgs);

        // Fails with an empty MatchPatternSet
        assert.property(frequentVisitsListener._matchPatternSet, "patterns");

        // Second try is succesful
        frequentVisitsListener.init(_triggerHandler, hosts, ["foo"]);

        assert.property(frequentVisitsListener._matchPatternSet, "patterns");
        assert.isTrue(
          frequentVisitsListener._matchPatternSet.patterns.has("foo")
        );
      });
    });
  });

  describe("nthTabClosed", () => {
    describe("#init", () => {
      beforeEach(() => {
        nthTabClosedListener.init(triggerHandler);
      });
      afterEach(() => {
        nthTabClosedListener.uninit();
      });

      it("should set ._initialized to true and save the triggerHandler", () => {
        assert.ok(nthTabClosedListener._initialized);
        assert.equal(nthTabClosedListener._triggerHandler, triggerHandler);
      });

      it("if already initialised, it should only update the trigger handler", () => {
        const newTriggerHandler = () => {};
        nthTabClosedListener.init(newTriggerHandler);
        assert.ok(nthTabClosedListener._initialized);
        assert.equal(nthTabClosedListener._triggerHandler, newTriggerHandler);
      });

      it("should add an event listeners to all existing browser windows", () => {
        assert.calledOnce(existingWindow.addEventListener);
        assert.calledWith(existingWindow.addEventListener, "TabClose");
      });
    });

    describe("#uninit", () => {
      beforeEach(async () => {
        nthTabClosedListener.init(triggerHandler);
        nthTabClosedListener.uninit();
      });
      it("should set ._initialized to false and clear the triggerHandler, closed tabs count", () => {
        assert.notOk(nthTabClosedListener._initialized);
        assert.equal(nthTabClosedListener._triggerHandler, null);
        assert.equal(nthTabClosedListener._closedTabs, 0);
      });

      it("should do nothing if already uninitialised", () => {
        nthTabClosedListener.uninit();
        assert.notOk(nthTabClosedListener._initialized);
      });

      it("should remove event listeners from all existing browser windows", () => {
        assert.calledOnce(existingWindow.removeEventListener);
      });
    });
  });

  describe("activityAfterIdle", () => {
    let addObsStub;
    let removeObsStub;
    describe("#init", () => {
      beforeEach(() => {
        addObsStub = sandbox.stub(global.Services.obs, "addObserver");
        sandbox
          .stub(global.Services.wm, "getEnumerator")
          .returns([{ closed: false, document: { hidden: false } }]);
        idleListener.init(triggerHandler);
      });
      afterEach(() => {
        idleListener.uninit();
      });

      it("should set ._initialized to true and save the triggerHandler", () => {
        assert.ok(idleListener._initialized);
        assert.equal(idleListener._triggerHandler, triggerHandler);
      });

      it("if already initialised, it should only update the trigger handler", () => {
        const newTriggerHandler = () => {};
        idleListener.init(newTriggerHandler);
        assert.ok(idleListener._initialized);
        assert.equal(idleListener._triggerHandler, newTriggerHandler);
      });

      it("should add observers for idle and activity", () => {
        assert.called(addObsStub);
      });

      it("should add event listeners to all existing browser windows", () => {
        assert.called(existingWindow.addEventListener);
      });
    });

    describe("#uninit", () => {
      beforeEach(async () => {
        removeObsStub = sandbox.stub(global.Services.obs, "removeObserver");
        sandbox.stub(global.Services.wm, "getEnumerator").returns([]);
        idleListener.init(triggerHandler);
        idleListener.uninit();
      });
      it("should set ._initialized to false and clear the triggerHandler and timestamps", () => {
        assert.notOk(idleListener._initialized);
        assert.equal(idleListener._triggerHandler, null);
        assert.equal(idleListener._quietSince, null);
      });

      it("should do nothing if already uninitialised", () => {
        idleListener.uninit();
        assert.notOk(idleListener._initialized);
      });

      it("should remove observers for idle and activity", () => {
        assert.called(removeObsStub);
      });

      it("should remove event listeners from all existing browser windows", () => {
        assert.called(existingWindow.removeEventListener);
      });
    });
  });

  describe("formAutofill", () => {
    let addObsStub;
    let removeObsStub;
    describe("#init", () => {
      beforeEach(() => {
        addObsStub = sandbox.stub(global.Services.obs, "addObserver");
        formAutofillListener.init(triggerHandler);
      });
      afterEach(() => {
        formAutofillListener.uninit();
      });

      it("should set ._initialized to true and save the triggerHandler", () => {
        assert.ok(formAutofillListener._initialized);
        assert.equal(formAutofillListener._triggerHandler, triggerHandler);
      });

      it("if already initialised, it should only update the trigger handler", () => {
        const newTriggerHandler = () => {};
        formAutofillListener.init(newTriggerHandler);
        assert.ok(formAutofillListener._initialized);
        assert.equal(formAutofillListener._triggerHandler, newTriggerHandler);
      });

      it(`should add observer for ${formAutofillListener._topic}`, () => {
        assert.called(addObsStub);
      });
    });

    describe("#uninit", () => {
      beforeEach(async () => {
        removeObsStub = sandbox.stub(global.Services.obs, "removeObserver");
        formAutofillListener.init(triggerHandler);
        formAutofillListener.uninit();
      });

      it("should set ._initialized to false and clear the triggerHandler", () => {
        assert.notOk(formAutofillListener._initialized);
        assert.equal(formAutofillListener._triggerHandler, null);
      });

      it("should do nothing if already uninitialised", () => {
        formAutofillListener.uninit();
        assert.notOk(formAutofillListener._initialized);
      });

      it(`should remove observers for ${formAutofillListener._topic}`, () => {
        assert.called(removeObsStub);
      });
    });
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

      it("should add tab progress listeners to all existing browser windows", () => {
        assert.calledOnce(existingWindow.gBrowser.addTabsProgressListener);
        assert.calledWithExactly(
          existingWindow.gBrowser.addTabsProgressListener,
          openURLListener
        );
      });

      it("if already initialised, should only update the trigger handler and add the new hosts", () => {
        const newHosts = ["www.example.com"];
        const newTriggerHandler = () => {};
        existingWindow.gBrowser.addTabsProgressListener.reset();

        openURLListener.init(newTriggerHandler, newHosts);
        assert.ok(openURLListener._initialized);
        assert.deepEqual(
          openURLListener._hosts,
          new Set([...hosts, ...newHosts])
        );
        assert.equal(openURLListener._triggerHandler, newTriggerHandler);
        assert.notCalled(existingWindow.gBrowser.addTabsProgressListener);
      });
    });

    describe("#uninit", () => {
      beforeEach(async () => {
        openURLListener.init(triggerHandler, hosts);
        openURLListener.uninit();
      });

      it("should set ._initialized to false and clear the triggerHandler and hosts", () => {
        assert.notOk(openURLListener._initialized);
        assert.equal(openURLListener._hosts, null);
        assert.equal(openURLListener._triggerHandler, null);
      });

      it("should remove tab progress listeners from all existing browser windows", () => {
        assert.calledOnce(existingWindow.gBrowser.removeTabsProgressListener);
        assert.calledWithExactly(
          existingWindow.gBrowser.removeTabsProgressListener,
          openURLListener
        );
      });

      it("should do nothing if already uninitialised", () => {
        existingWindow.gBrowser.removeTabsProgressListener.reset();

        openURLListener.uninit();
        assert.notOk(openURLListener._initialized);
        assert.notCalled(existingWindow.gBrowser.removeTabsProgressListener);
      });
    });

    describe("#onLocationChange", () => {
      afterEach(() => {
        openURLListener.uninit();
        frequentVisitsListener.uninit();
      });

      it("should call the ._triggerHandler with the right arguments", () => {
        const newTriggerHandler = sinon.stub();
        openURLListener.init(newTriggerHandler, hosts);

        const browser = {};
        const webProgress = { isTopLevel: true };
        const location = "www.mozilla.org";
        openURLListener.onLocationChange(browser, webProgress, undefined, {
          host: location,
          spec: location,
        });
        assert.calledOnce(newTriggerHandler);
        assert.calledWithExactly(newTriggerHandler, browser, {
          id: "openURL",
          param: { host: "www.mozilla.org", url: "www.mozilla.org" },
          context: { visitsCount: 1 },
        });
      });
      it("should call triggerHandler for a redirect (openURL + frequentVisits)", () => {
        for (let trigger of [openURLListener, frequentVisitsListener]) {
          const newTriggerHandler = sinon.stub();
          trigger.init(newTriggerHandler, hosts);

          const browser = {};
          const webProgress = { isTopLevel: true };
          const aLocationURI = {
            host: "subdomain.mozilla.org",
            spec: "subdomain.mozilla.org",
          };
          const aRequest = {
            QueryInterface: sandbox.stub().returns({
              originalURI: { spec: "www.mozilla.org", host: "www.mozilla.org" },
            }),
          };
          trigger.onLocationChange(
            browser,
            webProgress,
            aRequest,
            aLocationURI
          );
          assert.calledOnce(aRequest.QueryInterface);
          assert.calledOnce(newTriggerHandler);
        }
      });
      it("should call triggerHandler with the right arguments (redirect)", () => {
        const newTriggerHandler = sinon.stub();
        openURLListener.init(newTriggerHandler, hosts);

        const browser = {};
        const webProgress = { isTopLevel: true };
        const aLocationURI = {
          host: "subdomain.mozilla.org",
          spec: "subdomain.mozilla.org",
        };
        const aRequest = {
          QueryInterface: sandbox.stub().returns({
            originalURI: { spec: "www.mozilla.org", host: "www.mozilla.org" },
          }),
        };
        openURLListener.onLocationChange(
          browser,
          webProgress,
          aRequest,
          aLocationURI
        );
        assert.calledWithExactly(newTriggerHandler, browser, {
          id: "openURL",
          param: { host: "www.mozilla.org", url: "www.mozilla.org" },
          context: { visitsCount: 1 },
        });
      });
      it("should call triggerHandler for a redirect (openURL + frequentVisits)", () => {
        for (let trigger of [openURLListener, frequentVisitsListener]) {
          const newTriggerHandler = sinon.stub();
          trigger.init(newTriggerHandler, hosts);

          const browser = {};
          const webProgress = { isTopLevel: true };
          const aLocationURI = {
            host: "subdomain.mozilla.org",
            spec: "subdomain.mozilla.org",
          };
          const aRequest = {
            QueryInterface: sandbox.stub().returns({
              originalURI: { spec: "www.mozilla.org", host: "www.mozilla.org" },
            }),
          };
          trigger.onLocationChange(
            browser,
            webProgress,
            aRequest,
            aLocationURI
          );
          assert.calledOnce(aRequest.QueryInterface);
          assert.calledOnce(newTriggerHandler);
        }
      });
      it("should call triggerHandler with the right arguments (redirect)", () => {
        const newTriggerHandler = sinon.stub();
        openURLListener.init(newTriggerHandler, hosts);

        const browser = {};
        const webProgress = { isTopLevel: true };
        const aLocationURI = {
          host: "subdomain.mozilla.org",
          spec: "subdomain.mozilla.org",
        };
        const aRequest = {
          QueryInterface: sandbox.stub().returns({
            originalURI: { spec: "www.mozilla.org", host: "www.mozilla.org" },
          }),
        };
        openURLListener.onLocationChange(
          browser,
          webProgress,
          aRequest,
          aLocationURI
        );
        assert.calledWithExactly(newTriggerHandler, browser, {
          id: "openURL",
          param: { host: "www.mozilla.org", url: "www.mozilla.org" },
          context: { visitsCount: 1 },
        });
      });
      it("should fail for subdomains (not redirect)", () => {
        const newTriggerHandler = sinon.stub();
        openURLListener.init(newTriggerHandler, hosts);

        const browser = {};
        const webProgress = { isTopLevel: true };
        const aLocationURI = {
          host: "subdomain.mozilla.org",
          spec: "subdomain.mozilla.org",
        };
        const aRequest = {
          QueryInterface: sandbox.stub().returns({
            originalURI: {
              spec: "subdomain.mozilla.org",
              host: "subdomain.mozilla.org",
            },
          }),
        };
        openURLListener.onLocationChange(
          browser,
          webProgress,
          aRequest,
          aLocationURI
        );
        assert.calledOnce(aRequest.QueryInterface);
        assert.notCalled(newTriggerHandler);
      });
    });
  });

  describe("cookieBannerDetected", () => {
    describe("#init", () => {
      beforeEach(() => {
        cookieBannerDetectedListener.init(triggerHandler);
      });
      afterEach(() => {
        cookieBannerDetectedListener.uninit();
      });

      it("should set ._initialized to true and save the triggerHandler", () => {
        assert.ok(cookieBannerDetectedListener._initialized);
        assert.equal(
          cookieBannerDetectedListener._triggerHandler,
          triggerHandler
        );
      });

      it("if already initialised, it should only update the trigger handler", () => {
        const newTriggerHandler = () => {};
        cookieBannerDetectedListener.init(newTriggerHandler);
        assert.ok(cookieBannerDetectedListener._initialized);
        assert.equal(
          cookieBannerDetectedListener._triggerHandler,
          newTriggerHandler
        );
      });

      it("should add an event listeners to all existing browser windows", () => {
        assert.calledOnce(existingWindow.addEventListener);
      });
    });
    describe("#uninit", () => {
      beforeEach(async () => {
        cookieBannerDetectedListener.init(triggerHandler);
        cookieBannerDetectedListener.uninit();
      });
      it("should set ._initialized to false and clear the triggerHandler and timestamps", () => {
        assert.notOk(cookieBannerDetectedListener._initialized);
        assert.equal(cookieBannerDetectedListener._triggerHandler, null);
      });

      it("should do nothing if already uninitialised", () => {
        cookieBannerDetectedListener.uninit();
        assert.notOk(cookieBannerDetectedListener._initialized);
      });

      it("should remove event listeners from all existing browser windows", () => {
        assert.called(existingWindow.removeEventListener);
      });
    });
  });

  describe("cookieBannerHandled", () => {
    describe("#init", () => {
      beforeEach(() => {
        cookieBannerHandledListener.init(triggerHandler);
      });
      afterEach(() => {
        cookieBannerHandledListener.uninit();
      });

      it("should set ._initialized to true and save the triggerHandler", () => {
        assert.ok(cookieBannerHandledListener._initialized);
        assert.equal(
          cookieBannerHandledListener._triggerHandler,
          triggerHandler
        );
      });

      it("if already initialised, it should only update the trigger handler", () => {
        const newTriggerHandler = () => {};
        cookieBannerHandledListener.init(newTriggerHandler);
        assert.ok(cookieBannerHandledListener._initialized);
        assert.equal(
          cookieBannerHandledListener._triggerHandler,
          newTriggerHandler
        );
      });

      it("should add an event listeners to all existing browser windows", () => {
        assert.calledOnce(existingWindow.addEventListener);
      });
    });
    describe("#uninit", () => {
      beforeEach(async () => {
        cookieBannerHandledListener.init(triggerHandler);
        cookieBannerHandledListener.uninit();
      });
      it("should set ._initialized to false and clear the triggerHandler and timestamps", () => {
        assert.notOk(cookieBannerHandledListener._initialized);
        assert.equal(cookieBannerHandledListener._triggerHandler, null);
      });

      it("should do nothing if already uninitialised", () => {
        cookieBannerHandledListener.uninit();
        assert.notOk(cookieBannerHandledListener._initialized);
      });

      it("should remove event listeners from all existing browser windows", () => {
        assert.called(existingWindow.removeEventListener);
      });
    });
  });
});
