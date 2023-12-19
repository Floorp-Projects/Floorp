/* global Services */
import {
  actionCreators as ac,
  actionTypes as at,
} from "common/Actions.sys.mjs";
import {
  ASRouterEventPing,
  BasePing,
  SessionPing,
  UserEventPing,
} from "test/schemas/pings";
import { FAKE_GLOBAL_PREFS, GlobalOverrider } from "test/unit/utils";
import { ASRouterPreferences } from "lib/ASRouterPreferences.jsm";
import injector from "inject!lib/TelemetryFeed.jsm";
import { MESSAGE_TYPE_HASH as msg } from "common/ActorConstants.sys.mjs";

const FAKE_UUID = "{foo-123-foo}";
const FAKE_ROUTER_MESSAGE_PROVIDER = [{ id: "cfr", enabled: true }];
const FAKE_TELEMETRY_ID = "foo123";

// eslint-disable-next-line max-statements
describe("TelemetryFeed", () => {
  let globals;
  let sandbox;
  let expectedUserPrefs;
  let browser = {
    getAttribute() {
      return "true";
    },
  };
  let instance;
  let clock;
  let fakeHomePageUrl;
  let fakeHomePage;
  let fakeExtensionSettingsStore;
  let ExperimentAPI = { getExperimentMetaData: () => {} };
  class UTEventReporting {
    sendUserEvent() {}
    sendSessionEndEvent() {}
    uninit() {}
  }

  // Reset the global prefs before importing the `TelemetryFeed` module, to
  // avoid a coverage miss caused by preference pollution when this test and
  // `ActivityStream.test.js` are run together.
  //
  // The `TelemetryFeed` module defines a lazy `contextId` getter, which the
  // `XPCOMUtils.defineLazyGetter` mock (defined in `unit-entry.js`) executes
  // immediately, as soon as the module is imported.
  //
  // If this test runs first, there's no coverage miss: this test will load
  // the `TelemetryFeed` module and run the lazy `contextId` getter, which will
  // generate a fake context ID and store it in `FAKE_GLOBAL_PREFS`, covering
  // all branches in the module. When `ActivityStream.test.js` runs, it'll load
  // `TelemetryFeed` and run the lazy getter a second time, which will use the
  // existing fake context ID from `FAKE_GLOBAL_PREFS` instead of generating a
  // new one.
  //
  // But, if `ActivityStream.test.js` runs first, then loading `TelemetryFeed` a
  // second time as part of this test will use the existing fake context ID from
  // `FAKE_GLOBAL_PREFS`, missing coverage for the branch to generate a new
  // context ID.
  FAKE_GLOBAL_PREFS.clear();

  const {
    TelemetryFeed,
    USER_PREFS_ENCODING,
    PREF_IMPRESSION_ID,
    TELEMETRY_PREF,
    EVENTS_TELEMETRY_PREF,
  } = injector({
    "lib/UTEventReporting.sys.mjs": { UTEventReporting },
  });

  beforeEach(() => {
    globals = new GlobalOverrider();
    sandbox = globals.sandbox;
    clock = sinon.useFakeTimers();
    fakeHomePageUrl = "about:home";
    fakeHomePage = {
      get() {
        return fakeHomePageUrl;
      },
    };
    fakeExtensionSettingsStore = {
      initialize() {
        return Promise.resolve();
      },
      getSetting() {},
    };
    sandbox.spy(global.console, "error");
    globals.set("AboutNewTab", {
      newTabURLOverridden: false,
      newTabURL: "",
    });
    globals.set("pktApi", {
      isUserLoggedIn: () => true,
    });
    globals.set("HomePage", fakeHomePage);
    globals.set("ExtensionSettingsStore", fakeExtensionSettingsStore);
    globals.set("UTEventReporting", UTEventReporting);
    globals.set("ClientID", {
      getClientID: sandbox.spy(async () => FAKE_TELEMETRY_ID),
    });
    globals.set("ExperimentAPI", ExperimentAPI);

    sandbox
      .stub(ASRouterPreferences, "providers")
      .get(() => FAKE_ROUTER_MESSAGE_PROVIDER);
    instance = new TelemetryFeed();
  });
  afterEach(() => {
    clock.restore();
    globals.restore();
    FAKE_GLOBAL_PREFS.clear();
    ASRouterPreferences.uninit();
  });
  describe("#init", () => {
    it("should create an instance", () => {
      const testInstance = new TelemetryFeed();
      assert.isDefined(testInstance);
    });
    it("should add .utEvents, a UTEventReporting instance", () => {
      assert.instanceOf(instance.utEvents, UTEventReporting);
    });
    it("should make this.browserOpenNewtabStart() observe browser-open-newtab-start", () => {
      sandbox.spy(Services.obs, "addObserver");

      instance.init();

      assert.calledOnce(Services.obs.addObserver);
      assert.calledWithExactly(
        Services.obs.addObserver,
        instance.browserOpenNewtabStart,
        "browser-open-newtab-start"
      );
    });
    it("should create impression id if none exists", () => {
      assert.equal(instance._impressionId, FAKE_UUID);
    });
    it("should set impression id if it exists", () => {
      FAKE_GLOBAL_PREFS.set(PREF_IMPRESSION_ID, "fakeImpressionId");
      assert.equal(new TelemetryFeed()._impressionId, "fakeImpressionId");
    });
    describe("telemetry pref changes from false to true", () => {
      beforeEach(() => {
        FAKE_GLOBAL_PREFS.set(TELEMETRY_PREF, false);
        instance = new TelemetryFeed();

        assert.propertyVal(instance, "telemetryEnabled", false);
      });

      it("should set the enabled property to true", () => {
        instance._prefs.set(TELEMETRY_PREF, true);

        assert.propertyVal(instance, "telemetryEnabled", true);
      });
    });
    describe("events telemetry pref changes from false to true", () => {
      beforeEach(() => {
        FAKE_GLOBAL_PREFS.set(EVENTS_TELEMETRY_PREF, false);
        instance = new TelemetryFeed();

        assert.propertyVal(instance, "eventTelemetryEnabled", false);
      });

      it("should set the enabled property to true", () => {
        instance._prefs.set(EVENTS_TELEMETRY_PREF, true);

        assert.propertyVal(instance, "eventTelemetryEnabled", true);
      });
    });
    it("should set two scalars for deletion-request", () => {
      sandbox.spy(Services.telemetry, "scalarSet");

      instance.init();

      assert.calledTwice(Services.telemetry.scalarSet);

      // impression_id
      let [type, value] = Services.telemetry.scalarSet.firstCall.args;
      assert.equal(type, "deletion.request.impression_id");
      assert.equal(value, instance._impressionId);

      // context_id
      [type, value] = Services.telemetry.scalarSet.secondCall.args;
      assert.equal(type, "deletion.request.context_id");
      assert.equal(value, FAKE_UUID);
    });
    describe("#_beginObservingNewtabPingPrefs", () => {
      it("should record initial metrics from newtab prefs", () => {
        FAKE_GLOBAL_PREFS.set(
          "browser.newtabpage.activity-stream.feeds.topsites",
          true
        );
        FAKE_GLOBAL_PREFS.set(
          "browser.newtabpage.activity-stream.topSitesRows",
          3
        );
        FAKE_GLOBAL_PREFS.set(
          "browser.topsites.blockedSponsors",
          '["mozilla"]'
        );

        sandbox.spy(Glean.topsites.enabled, "set");
        sandbox.spy(Glean.topsites.rows, "set");
        sandbox.spy(Glean.newtab.blockedSponsors, "set");

        instance = new TelemetryFeed();
        instance.init();

        assert.calledOnce(Glean.topsites.enabled.set);
        assert.calledWith(Glean.topsites.enabled.set, true);
        assert.calledOnce(Glean.topsites.rows.set);
        assert.calledWith(Glean.topsites.rows.set, 3);
        assert.calledOnce(Glean.newtab.blockedSponsors.set);
        assert.calledWith(Glean.newtab.blockedSponsors.set, ["mozilla"]);
      });

      it("should not record blocked sponsor metrics when bad json string is passed", () => {
        FAKE_GLOBAL_PREFS.set("browser.topsites.blockedSponsors", "BAD[JSON]");

        sandbox.spy(Glean.newtab.blockedSponsors, "set");

        instance = new TelemetryFeed();
        instance.init();

        assert.notCalled(Glean.newtab.blockedSponsors.set);
      });

      it("should record new metrics for newtab pref changes", () => {
        FAKE_GLOBAL_PREFS.set(
          "browser.newtabpage.activity-stream.topSitesRows",
          3
        );
        FAKE_GLOBAL_PREFS.set("browser.topsites.blockedSponsors", "[]");
        sandbox.spy(Glean.topsites.rows, "set");
        sandbox.spy(Glean.newtab.blockedSponsors, "set");

        instance = new TelemetryFeed();
        instance.init();

        Services.prefs.setIntPref(
          "browser.newtabpage.activity-stream.topSitesRows",
          2
        );

        Services.prefs.setStringPref(
          "browser.topsites.blockedSponsors",
          '["mozilla"]'
        );

        assert.calledTwice(Glean.topsites.rows.set);
        assert.calledWith(Glean.topsites.rows.set.firstCall, 3);
        assert.calledWith(Glean.topsites.rows.set.secondCall, 2);
        assert.calledWith(Glean.newtab.blockedSponsors.set.firstCall, []);
        assert.calledWith(Glean.newtab.blockedSponsors.set.secondCall, [
          "mozilla",
        ]);
      });
      it("should record pref_changed events for topsites pref changes", () => {
        FAKE_GLOBAL_PREFS.set(
          "browser.newtabpage.activity-stream.feeds.topsites",
          false
        );
        FAKE_GLOBAL_PREFS.set(
          "browser.newtabpage.activity-stream.showSponsoredTopSites",
          true
        );
        sandbox.spy(Glean.topsites.enabled, "set");
        sandbox.spy(Glean.topsites.sponsoredEnabled, "set");
        sandbox.spy(Glean.topsites.prefChanged, "record");

        instance = new TelemetryFeed();
        instance.init();

        Services.prefs.setBoolPref(
          "browser.newtabpage.activity-stream.feeds.topsites",
          true
        );
        Services.prefs.setBoolPref(
          "browser.newtabpage.activity-stream.showSponsoredTopSites",
          false
        );

        assert.calledTwice(Glean.topsites.prefChanged.record);
        assert.deepEqual(Glean.topsites.prefChanged.record.firstCall.args[0], {
          pref_name: "browser.newtabpage.activity-stream.feeds.topsites",
          new_value: true,
        });
        assert.deepEqual(Glean.topsites.prefChanged.record.secondCall.args[0], {
          pref_name: "browser.newtabpage.activity-stream.showSponsoredTopSites",
          new_value: false,
        });
      });
      it("should ignore changes to other prefs", () => {
        FAKE_GLOBAL_PREFS.set("some.other.pref", 123);
        FAKE_GLOBAL_PREFS.set(
          "browser.newtabpage.activity-stream.impressionId",
          "{foo-123-foo}"
        );

        instance = new TelemetryFeed();
        instance.init();

        Services.prefs.setIntPref("some.other.pref", 456);
        Services.prefs.setCharPref(
          "browser.newtabpage.activity-stream.impressionId",
          "{foo-456-foo}"
        );
      });
    });
  });
  describe("#addSession", () => {
    it("should add a session and return it", () => {
      const session = instance.addSession("foo");

      assert.equal(instance.sessions.get("foo"), session);
    });
    it("should set the session_id", () => {
      sandbox.spy(Services.uuid, "generateUUID");

      const session = instance.addSession("foo");

      assert.calledOnce(Services.uuid.generateUUID);
      assert.equal(
        session.session_id,
        Services.uuid.generateUUID.firstCall.returnValue
      );
    });
    it("should set the page if a url parameter is given", () => {
      const session = instance.addSession("foo", "about:monkeys");

      assert.propertyVal(session, "page", "about:monkeys");
    });
    it("should set the page prop to 'unknown' if no URL parameter given", () => {
      const session = instance.addSession("foo");

      assert.propertyVal(session, "page", "unknown");
    });
    it("should set the perf type when lacking timestamp", () => {
      const session = instance.addSession("foo");

      assert.propertyVal(session.perf, "load_trigger_type", "unexpected");
    });
    it("should set load_trigger_type to first_window_opened on the first about:home seen", () => {
      const session = instance.addSession("foo", "about:home");

      assert.propertyVal(
        session.perf,
        "load_trigger_type",
        "first_window_opened"
      );
    });
    it("should not set load_trigger_type to first_window_opened on the second about:home seen", () => {
      instance.addSession("foo", "about:home");

      const session2 = instance.addSession("foo", "about:home");

      assert.notPropertyVal(
        session2.perf,
        "load_trigger_type",
        "first_window_opened"
      );
    });
    it("should set load_trigger_ts to the value of the process start timestamp", () => {
      const session = instance.addSession("foo", "about:home");

      assert.propertyVal(session.perf, "load_trigger_ts", 1588010448000);
    });
    it("should create a valid session ping on the first about:home seen", () => {
      // Add a session
      const portID = "foo";
      const session = instance.addSession(portID, "about:home");

      // Create a ping referencing the session
      const ping = instance.createSessionEndEvent(session);
      assert.validate(ping, SessionPing);
    });
    it("should be a valid ping with the data_late_by_ms perf", () => {
      // Add a session
      const portID = "foo";
      const session = instance.addSession(portID, "about:home");
      instance.saveSessionPerfData("foo", { topsites_data_late_by_ms: 10 });
      instance.saveSessionPerfData("foo", { highlights_data_late_by_ms: 20 });

      // Create a ping referencing the session
      const ping = instance.createSessionEndEvent(session);
      assert.validate(ping, SessionPing);
      assert.propertyVal(
        instance.sessions.get("foo").perf,
        "highlights_data_late_by_ms",
        20
      );
      assert.propertyVal(
        instance.sessions.get("foo").perf,
        "topsites_data_late_by_ms",
        10
      );
    });
    it("should be a valid ping with the topsites stats perf", () => {
      // Add a session
      const portID = "foo";
      const session = instance.addSession(portID, "about:home");
      instance.saveSessionPerfData("foo", {
        topsites_icon_stats: {
          custom_screenshot: 0,
          screenshot_with_icon: 2,
          screenshot: 1,
          tippytop: 2,
          rich_icon: 1,
          no_image: 0,
        },
        topsites_pinned: 3,
        topsites_search_shortcuts: 2,
      });

      // Create a ping referencing the session
      const ping = instance.createSessionEndEvent(session);
      assert.validate(ping, SessionPing);
      assert.propertyVal(
        instance.sessions.get("foo").perf.topsites_icon_stats,
        "screenshot_with_icon",
        2
      );
      assert.equal(instance.sessions.get("foo").perf.topsites_pinned, 3);
      assert.equal(
        instance.sessions.get("foo").perf.topsites_search_shortcuts,
        2
      );
    });
  });

  describe("#browserOpenNewtabStart", () => {
    it("should call ChromeUtils.addProfilerMarker with browser-open-newtab-start", () => {
      globals.set("ChromeUtils", {
        addProfilerMarker: sandbox.stub(),
      });

      sandbox.stub(global.Cu, "now").returns(12345);

      instance.browserOpenNewtabStart();

      assert.calledOnce(ChromeUtils.addProfilerMarker);
      assert.calledWithExactly(
        ChromeUtils.addProfilerMarker,
        "UserTiming",
        12345,
        "browser-open-newtab-start"
      );
    });
  });

  describe("#endSession", () => {
    it("should not throw if there is no session for the given port ID", () => {
      assert.doesNotThrow(() => instance.endSession("doesn't exist"));
    });
    it("should add a session_duration integer if there is a visibility_event_rcvd_ts", () => {
      const session = instance.addSession("foo");
      session.perf.visibility_event_rcvd_ts = 444.4732;

      instance.endSession("foo");

      assert.isNumber(session.session_duration);
      assert.ok(
        Number.isInteger(session.session_duration),
        "session_duration should be an integer"
      );
    });
    it("shouldn't send session ping if there's no visibility_event_rcvd_ts", () => {
      instance.addSession("foo");

      instance.endSession("foo");

      assert.isFalse(instance.sessions.has("foo"));
    });
    it("should remove the session from .sessions", () => {
      instance.addSession("foo");

      instance.endSession("foo");

      assert.isFalse(instance.sessions.has("foo"));
    });
    it("should call createSessionSendEvent with the sesssion", () => {
      FAKE_GLOBAL_PREFS.set(TELEMETRY_PREF, true);
      FAKE_GLOBAL_PREFS.set(EVENTS_TELEMETRY_PREF, true);
      instance = new TelemetryFeed();

      sandbox.stub(instance, "createSessionEndEvent");
      sandbox.stub(instance.utEvents, "sendSessionEndEvent");
      const session = instance.addSession("foo");
      session.perf.visibility_event_rcvd_ts = 444.4732;

      instance.endSession("foo");

      assert.calledWith(instance.createSessionEndEvent, session);

      let sessionEndEvent =
        instance.createSessionEndEvent.firstCall.returnValue;
      assert.calledWith(instance.utEvents.sendSessionEndEvent, sessionEndEvent);
    });
  });
  describe("ping creators", () => {
    beforeEach(() => {
      for (const pref of Object.keys(USER_PREFS_ENCODING)) {
        FAKE_GLOBAL_PREFS.set(pref, true);
        expectedUserPrefs |= USER_PREFS_ENCODING[pref];
      }
      instance.init();
    });
    describe("#createPing", () => {
      it("should create a valid base ping without a session if no portID is supplied", async () => {
        const ping = await instance.createPing();
        assert.validate(ping, BasePing);
        assert.notProperty(ping, "session_id");
        assert.notProperty(ping, "page");
      });
      it("should create a valid base ping with session info if a portID is supplied", async () => {
        // Add a session
        const portID = "foo";
        instance.addSession(portID, "about:home");
        const sessionID = instance.sessions.get(portID).session_id;

        // Create a ping referencing the session
        const ping = await instance.createPing(portID);
        assert.validate(ping, BasePing);

        // Make sure we added the right session-related stuff to the ping
        assert.propertyVal(ping, "session_id", sessionID);
        assert.propertyVal(ping, "page", "about:home");
      });
      it("should create an unexpected base ping if no session yet portID is supplied", async () => {
        const ping = await instance.createPing("foo");

        assert.validate(ping, BasePing);
        assert.propertyVal(ping, "page", "unknown");
        assert.propertyVal(
          instance.sessions.get("foo").perf,
          "load_trigger_type",
          "unexpected"
        );
      });
      it("should create a base ping with user_prefs", async () => {
        const ping = await instance.createPing("foo");

        assert.validate(ping, BasePing);
        assert.propertyVal(ping, "user_prefs", expectedUserPrefs);
      });
    });
    describe("#createUserEvent", () => {
      it("should create a valid event", async () => {
        const portID = "foo";
        const data = { source: "TOP_SITES", event: "CLICK" };
        const action = ac.AlsoToMain(ac.UserEvent(data), portID);
        const session = instance.addSession(portID);

        const ping = await instance.createUserEvent(action);

        // Is it valid?
        assert.validate(ping, UserEventPing);
        // Does it have the right session_id?
        assert.propertyVal(ping, "session_id", session.session_id);
      });
    });
    describe("#createSessionEndEvent", () => {
      it("should create a valid event", async () => {
        const ping = await instance.createSessionEndEvent({
          session_id: FAKE_UUID,
          page: "about:newtab",
          session_duration: 12345,
          perf: {
            load_trigger_ts: 10,
            load_trigger_type: "menu_plus_or_keyboard",
            visibility_event_rcvd_ts: 20,
            is_preloaded: true,
          },
        });

        // Is it valid?
        assert.validate(ping, SessionPing);
        assert.propertyVal(ping, "session_id", FAKE_UUID);
        assert.propertyVal(ping, "page", "about:newtab");
        assert.propertyVal(ping, "session_duration", 12345);
      });
      it("should create a valid unexpected session event", async () => {
        const ping = await instance.createSessionEndEvent({
          session_id: FAKE_UUID,
          page: "about:newtab",
          session_duration: 12345,
          perf: {
            load_trigger_type: "unexpected",
            is_preloaded: true,
          },
        });

        // Is it valid?
        assert.validate(ping, SessionPing);
        assert.propertyVal(ping, "session_id", FAKE_UUID);
        assert.propertyVal(ping, "page", "about:newtab");
        assert.propertyVal(ping, "session_duration", 12345);
        assert.propertyVal(ping.perf, "load_trigger_type", "unexpected");
      });
    });
  });
  describe("#applyCFRPolicy", () => {
    it("should use client_id and message_id in prerelease", async () => {
      globals.set("UpdateUtils", {
        getUpdateChannel() {
          return "nightly";
        },
      });
      const data = {
        action: "cfr_user_event",
        event: "IMPRESSION",
        message_id: "cfr_message_01",
        bucket_id: "cfr_bucket_01",
      };
      const { ping, pingType } = await instance.applyCFRPolicy(data);

      assert.equal(pingType, "cfr");
      assert.isUndefined(ping.impression_id);
      assert.propertyVal(ping, "client_id", FAKE_TELEMETRY_ID);
      assert.propertyVal(ping, "bucket_id", "cfr_bucket_01");
      assert.propertyVal(ping, "message_id", "cfr_message_01");
    });
    it("should use impression_id and bucket_id in release", async () => {
      globals.set("UpdateUtils", {
        getUpdateChannel() {
          return "release";
        },
      });
      const data = {
        action: "cfr_user_event",
        event: "IMPRESSION",
        message_id: "cfr_message_01",
        bucket_id: "cfr_bucket_01",
      };
      const { ping, pingType } = await instance.applyCFRPolicy(data);

      assert.equal(pingType, "cfr");
      assert.isUndefined(ping.client_id);
      assert.propertyVal(ping, "impression_id", FAKE_UUID);
      assert.propertyVal(ping, "message_id", "n/a");
      assert.propertyVal(ping, "bucket_id", "cfr_bucket_01");
    });
    it("should use client_id and message_id in the experiment cohort in release", async () => {
      globals.set("UpdateUtils", {
        getUpdateChannel() {
          return "release";
        },
      });
      sandbox.stub(ExperimentAPI, "getExperimentMetaData").returns({
        slug: "SOME-CFR-EXP",
      });
      const data = {
        action: "cfr_user_event",
        event: "IMPRESSION",
        message_id: "cfr_message_01",
        bucket_id: "cfr_bucket_01",
      };
      const { ping, pingType } = await instance.applyCFRPolicy(data);

      assert.equal(pingType, "cfr");
      assert.isUndefined(ping.impression_id);
      assert.propertyVal(ping, "client_id", FAKE_TELEMETRY_ID);
      assert.propertyVal(ping, "bucket_id", "cfr_bucket_01");
      assert.propertyVal(ping, "message_id", "cfr_message_01");
    });
    it("should use impression_id and bucket_id in Private Browsing", async () => {
      globals.set("UpdateUtils", {
        getUpdateChannel() {
          return "release";
        },
      });
      const data = {
        action: "cfr_user_event",
        event: "IMPRESSION",
        is_private: true,
        message_id: "cfr_message_01",
        bucket_id: "cfr_bucket_01",
      };
      const { ping, pingType } = await instance.applyCFRPolicy(data);

      assert.equal(pingType, "cfr");
      assert.isUndefined(ping.client_id);
      assert.propertyVal(ping, "impression_id", FAKE_UUID);
      assert.propertyVal(ping, "message_id", "n/a");
      assert.propertyVal(ping, "bucket_id", "cfr_bucket_01");
    });
    it("should use client_id and message_id in the experiment cohort in Private Browsing", async () => {
      globals.set("UpdateUtils", {
        getUpdateChannel() {
          return "release";
        },
      });
      sandbox.stub(ExperimentAPI, "getExperimentMetaData").returns({
        slug: "SOME-CFR-EXP",
      });
      const data = {
        action: "cfr_user_event",
        event: "IMPRESSION",
        is_private: true,
        message_id: "cfr_message_01",
        bucket_id: "cfr_bucket_01",
      };
      const { ping, pingType } = await instance.applyCFRPolicy(data);

      assert.equal(pingType, "cfr");
      assert.isUndefined(ping.impression_id);
      assert.propertyVal(ping, "client_id", FAKE_TELEMETRY_ID);
      assert.propertyVal(ping, "bucket_id", "cfr_bucket_01");
      assert.propertyVal(ping, "message_id", "cfr_message_01");
    });
  });
  describe("#applyWhatsNewPolicy", () => {
    it("should set client_id and set pingType", async () => {
      const { ping, pingType } = await instance.applyWhatsNewPolicy({});

      assert.propertyVal(ping, "client_id", FAKE_TELEMETRY_ID);
      assert.equal(pingType, "whats-new-panel");
    });
  });
  describe("#applyInfoBarPolicy", () => {
    it("should set client_id and set pingType", async () => {
      const { ping, pingType } = await instance.applyInfoBarPolicy({});

      assert.propertyVal(ping, "client_id", FAKE_TELEMETRY_ID);
      assert.equal(pingType, "infobar");
    });
  });
  describe("#applyToastNotificationPolicy", () => {
    it("should set client_id and set pingType", async () => {
      const { ping, pingType } = await instance.applyToastNotificationPolicy(
        {}
      );

      assert.propertyVal(ping, "client_id", FAKE_TELEMETRY_ID);
      assert.equal(pingType, "toast_notification");
    });
  });
  describe("#applySpotlightPolicy", () => {
    it("should set client_id and set pingType", async () => {
      let pingData = { action: "foo" };
      const { ping, pingType } = await instance.applySpotlightPolicy(pingData);

      assert.propertyVal(ping, "client_id", FAKE_TELEMETRY_ID);
      assert.equal(pingType, "spotlight");
      assert.notProperty(ping, "action");
    });
  });
  describe("#applyMomentsPolicy", () => {
    it("should use client_id and message_id in prerelease", async () => {
      globals.set("UpdateUtils", {
        getUpdateChannel() {
          return "nightly";
        },
      });
      const data = {
        action: "moments_user_event",
        event: "IMPRESSION",
        message_id: "moments_message_01",
        bucket_id: "moments_bucket_01",
      };
      const { ping, pingType } = await instance.applyMomentsPolicy(data);

      assert.equal(pingType, "moments");
      assert.isUndefined(ping.impression_id);
      assert.propertyVal(ping, "client_id", FAKE_TELEMETRY_ID);
      assert.propertyVal(ping, "bucket_id", "moments_bucket_01");
      assert.propertyVal(ping, "message_id", "moments_message_01");
    });
    it("should use impression_id and bucket_id in release", async () => {
      globals.set("UpdateUtils", {
        getUpdateChannel() {
          return "release";
        },
      });
      const data = {
        action: "moments_user_event",
        event: "IMPRESSION",
        message_id: "moments_message_01",
        bucket_id: "moments_bucket_01",
      };
      const { ping, pingType } = await instance.applyMomentsPolicy(data);

      assert.equal(pingType, "moments");
      assert.isUndefined(ping.client_id);
      assert.propertyVal(ping, "impression_id", FAKE_UUID);
      assert.propertyVal(ping, "message_id", "n/a");
      assert.propertyVal(ping, "bucket_id", "moments_bucket_01");
    });
    it("should use client_id and message_id in the experiment cohort in release", async () => {
      globals.set("UpdateUtils", {
        getUpdateChannel() {
          return "release";
        },
      });
      sandbox.stub(ExperimentAPI, "getExperimentMetaData").returns({
        slug: "SOME-CFR-EXP",
      });
      const data = {
        action: "moments_user_event",
        event: "IMPRESSION",
        message_id: "moments_message_01",
        bucket_id: "moments_bucket_01",
      };
      const { ping, pingType } = await instance.applyMomentsPolicy(data);

      assert.equal(pingType, "moments");
      assert.isUndefined(ping.impression_id);
      assert.propertyVal(ping, "client_id", FAKE_TELEMETRY_ID);
      assert.propertyVal(ping, "bucket_id", "moments_bucket_01");
      assert.propertyVal(ping, "message_id", "moments_message_01");
    });
  });
  describe("#applyOnboardingPolicy", () => {
    it("should include client_id", async () => {
      const data = {
        action: "onboarding_user_event",
        event: "CLICK_BUTTION",
        message_id: "onboarding_message_01",
      };
      const { ping, pingType } = await instance.applyOnboardingPolicy(data);

      assert.equal(pingType, "onboarding");
      assert.propertyVal(ping, "client_id", FAKE_TELEMETRY_ID);
      assert.propertyVal(ping, "message_id", "onboarding_message_01");
      assert.propertyVal(ping, "browser_session_id", "fake_session_id");
    });
    it("should include page to event_context if there is a session", async () => {
      const data = {
        action: "onboarding_user_event",
        event: "CLICK_BUTTION",
        message_id: "onboarding_message_01",
      };
      const session = { page: "about:welcome" };
      const { ping, pingType } = await instance.applyOnboardingPolicy(
        data,
        session
      );

      assert.equal(pingType, "onboarding");
      assert.propertyVal(
        ping,
        "event_context",
        JSON.stringify({ page: "about:welcome" })
      );
      assert.propertyVal(ping, "message_id", "onboarding_message_01");
    });
    it("should not set page if it is not in ONBOARDING_ALLOWED_PAGE_VALUES", async () => {
      const data = {
        action: "onboarding_user_event",
        event: "CLICK_BUTTION",
        message_id: "onboarding_message_01",
      };
      const session = { page: "foo" };
      const { ping, pingType } = await instance.applyOnboardingPolicy(
        data,
        session
      );

      assert.calledOnce(global.console.error);
      assert.equal(pingType, "onboarding");
      assert.propertyVal(ping, "event_context", JSON.stringify({}));
      assert.propertyVal(ping, "message_id", "onboarding_message_01");
    });
    it("should append page to event_context if it is not empty", async () => {
      const data = {
        action: "onboarding_user_event",
        event: "CLICK_BUTTION",
        message_id: "onboarding_message_01",
        event_context: JSON.stringify({ foo: "bar" }),
      };
      const session = { page: "about:welcome" };
      const { ping, pingType } = await instance.applyOnboardingPolicy(
        data,
        session
      );

      assert.equal(pingType, "onboarding");
      assert.propertyVal(
        ping,
        "event_context",
        JSON.stringify({ foo: "bar", page: "about:welcome" })
      );
      assert.propertyVal(ping, "message_id", "onboarding_message_01");
    });
    it("should append page to event_context if it is not a JSON serialized string", async () => {
      const data = {
        action: "onboarding_user_event",
        event: "CLICK_BUTTION",
        message_id: "onboarding_message_01",
        event_context: "foo",
      };
      const session = { page: "about:welcome" };
      const { ping, pingType } = await instance.applyOnboardingPolicy(
        data,
        session
      );

      assert.equal(pingType, "onboarding");
      assert.propertyVal(
        ping,
        "event_context",
        JSON.stringify({ value: "foo", page: "about:welcome" })
      );
      assert.propertyVal(ping, "message_id", "onboarding_message_01");
    });
  });
  describe("#applyUndesiredEventPolicy", () => {
    it("should exclude client_id and use impression_id", () => {
      const data = {
        action: "asrouter_undesired_event",
        event: "RS_MISSING_DATA",
      };
      const { ping, pingType } = instance.applyUndesiredEventPolicy(data);

      assert.equal(pingType, "undesired-events");
      assert.isUndefined(ping.client_id);
      assert.propertyVal(ping, "impression_id", FAKE_UUID);
    });
  });
  describe("#createASRouterEvent", () => {
    it("should create a valid AS Router event", async () => {
      const data = {
        action: "cfr_user_event",
        event: "CLICK",
        message_id: "cfr_message_01",
      };
      const action = ac.ASRouterUserEvent(data);
      const { ping } = await instance.createASRouterEvent(action);

      assert.validate(ping, ASRouterEventPing);
      assert.propertyVal(ping, "event", "CLICK");
    });
    it("should call applyCFRPolicy if action equals to cfr_user_event", async () => {
      const data = {
        action: "cfr_user_event",
        event: "IMPRESSION",
        message_id: "cfr_message_01",
      };
      sandbox.stub(instance, "applyCFRPolicy");
      const action = ac.ASRouterUserEvent(data);
      await instance.createASRouterEvent(action);

      assert.calledOnce(instance.applyCFRPolicy);
    });
    it("should call applyOnboardingPolicy if action equals to onboarding_user_event", async () => {
      const data = {
        action: "onboarding_user_event",
        event: "CLICK_BUTTON",
        message_id: "onboarding_message_01",
      };
      sandbox.stub(instance, "applyOnboardingPolicy");
      const action = ac.ASRouterUserEvent(data);
      await instance.createASRouterEvent(action);

      assert.calledOnce(instance.applyOnboardingPolicy);
    });
    it("should call applyWhatsNewPolicy if action equals to whats-new-panel_user_event", async () => {
      const data = {
        action: "whats-new-panel_user_event",
        event: "CLICK_BUTTON",
        message_id: "whats-new-panel_message_01",
      };
      sandbox.stub(instance, "applyWhatsNewPolicy");
      const action = ac.ASRouterUserEvent(data);
      await instance.createASRouterEvent(action);

      assert.calledOnce(instance.applyWhatsNewPolicy);
    });
    it("should call applyMomentsPolicy if action equals to moments_user_event", async () => {
      const data = {
        action: "moments_user_event",
        event: "CLICK_BUTTON",
        message_id: "moments_message_01",
      };
      sandbox.stub(instance, "applyMomentsPolicy");
      const action = ac.ASRouterUserEvent(data);
      await instance.createASRouterEvent(action);

      assert.calledOnce(instance.applyMomentsPolicy);
    });
    it("should call applySpotlightPolicy if action equals to spotlight_user_event", async () => {
      const data = {
        action: "spotlight_user_event",
        event: "CLICK",
        message_id: "SPOTLIGHT_MESSAGE_93",
      };
      sandbox.stub(instance, "applySpotlightPolicy");
      const action = ac.ASRouterUserEvent(data);
      await instance.createASRouterEvent(action);

      assert.calledOnce(instance.applySpotlightPolicy);
    });
    it("should call applyToastNotificationPolicy if action equals to toast_notification_user_event", async () => {
      const data = {
        action: "toast_notification_user_event",
        event: "IMPRESSION",
        message_id: "TEST_TOAST_NOTIFICATION1",
      };
      sandbox.stub(instance, "applyToastNotificationPolicy");
      const action = ac.ASRouterUserEvent(data);
      await instance.createASRouterEvent(action);

      assert.calledOnce(instance.applyToastNotificationPolicy);
    });
    it("should call applyUndesiredEventPolicy if action equals to asrouter_undesired_event", async () => {
      const data = {
        action: "asrouter_undesired_event",
        event: "UNDESIRED_EVENT",
      };
      sandbox.stub(instance, "applyUndesiredEventPolicy");
      const action = ac.ASRouterUserEvent(data);
      await instance.createASRouterEvent(action);

      assert.calledOnce(instance.applyUndesiredEventPolicy);
    });
    it("should stringify event_context if it is an Object", async () => {
      const data = {
        action: "asrouter_undesired_event",
        event: "UNDESIRED_EVENT",
        event_context: { foo: "bar" },
      };
      const action = ac.ASRouterUserEvent(data);
      const { ping } = await instance.createASRouterEvent(action);

      assert.propertyVal(ping, "event_context", JSON.stringify({ foo: "bar" }));
    });
    it("should not stringify event_context if it is a String", async () => {
      const data = {
        action: "asrouter_undesired_event",
        event: "UNDESIRED_EVENT",
        event_context: "foo",
      };
      const action = ac.ASRouterUserEvent(data);
      const { ping } = await instance.createASRouterEvent(action);

      assert.propertyVal(ping, "event_context", "foo");
    });
  });
  describe("#sendUTEvent", () => {
    it("should call the UT event function passed in", async () => {
      FAKE_GLOBAL_PREFS.set(TELEMETRY_PREF, true);
      FAKE_GLOBAL_PREFS.set(EVENTS_TELEMETRY_PREF, true);
      const event = {};
      instance = new TelemetryFeed();
      sandbox.stub(instance.utEvents, "sendUserEvent");

      await instance.sendUTEvent(event, instance.utEvents.sendUserEvent);

      assert.calledWith(instance.utEvents.sendUserEvent, event);
    });
  });
  describe("#setLoadTriggerInfo", () => {
    it("should call saveSessionPerfData w/load_trigger_{ts,type} data", () => {
      sandbox.stub(global.Cu, "now").returns(12345);

      globals.set("ChromeUtils", {
        addProfilerMarker: sandbox.stub(),
      });

      instance.browserOpenNewtabStart();

      const stub = sandbox.stub(instance, "saveSessionPerfData");
      instance.addSession("port123");

      instance.setLoadTriggerInfo("port123");

      assert.calledWith(stub, "port123", {
        load_trigger_ts: 1588010448000 + 12345,
        load_trigger_type: "menu_plus_or_keyboard",
      });
    });

    it("should not call saveSessionPerfData when getting mark throws", () => {
      const stub = sandbox.stub(instance, "saveSessionPerfData");
      instance.addSession("port123");

      instance.setLoadTriggerInfo("port123");

      assert.notCalled(stub);
    });
  });

  describe("#saveSessionPerfData", () => {
    it("should update the given session with the given data", () => {
      instance.addSession("port123");
      assert.notProperty(instance.sessions.get("port123"), "fake_ts");
      const data = { fake_ts: 456, other_fake_ts: 789 };

      instance.saveSessionPerfData("port123", data);

      assert.include(instance.sessions.get("port123").perf, data);
    });

    it("should call setLoadTriggerInfo if data has visibility_event_rcvd_ts", () => {
      sandbox.stub(instance, "setLoadTriggerInfo");
      instance.addSession("port123");
      const data = { visibility_event_rcvd_ts: 444455 };

      instance.saveSessionPerfData("port123", data);

      assert.calledOnce(instance.setLoadTriggerInfo);
      assert.calledWithExactly(instance.setLoadTriggerInfo, "port123");
      assert.include(instance.sessions.get("port123").perf, data);
    });

    it("shouldn't call setLoadTriggerInfo if data has no visibility_event_rcvd_ts", () => {
      sandbox.stub(instance, "setLoadTriggerInfo");
      instance.addSession("port123");

      instance.saveSessionPerfData("port123", { monkeys_ts: 444455 });

      assert.notCalled(instance.setLoadTriggerInfo);
    });

    it("should not call setLoadTriggerInfo when url is about:home", () => {
      sandbox.stub(instance, "setLoadTriggerInfo");
      instance.addSession("port123", "about:home");
      const data = { visibility_event_rcvd_ts: 444455 };

      instance.saveSessionPerfData("port123", data);

      assert.notCalled(instance.setLoadTriggerInfo);
    });

    it("should call maybeRecordTopsitesPainted when url is about:home and topsites_first_painted_ts is given", () => {
      const topsites_first_painted_ts = 44455;
      const data = { topsites_first_painted_ts };
      const spy = sandbox.spy();

      sandbox.stub(Services.prefs, "getIntPref").returns(1);
      globals.set("AboutNewTab", {
        maybeRecordTopsitesPainted: spy,
      });
      instance.addSession("port123", "about:home");
      instance.saveSessionPerfData("port123", data);

      assert.calledOnce(spy);
      assert.calledWith(spy, topsites_first_painted_ts);
    });
    it("should record a Glean newtab.opened event with the correct visit_id when visibility event received", () => {
      const session_id = "decafc0ffee";
      const page = "about:newtab";
      const session = { page, perf: {}, session_id };
      const data = { visibility_event_rcvd_ts: 444455 };
      sandbox.stub(instance.sessions, "get").returns(session);

      sandbox.spy(Glean.newtab.opened, "record");
      instance.saveSessionPerfData("port123", data);

      assert.calledOnce(Glean.newtab.opened.record);
      assert.deepEqual(Glean.newtab.opened.record.firstCall.args[0], {
        newtab_visit_id: session_id,
        source: page,
      });
    });
  });
  describe("#uninit", () => {
    it("should call .utEvents.uninit", () => {
      const stub = sandbox.stub(instance.utEvents, "uninit");

      instance.uninit();

      assert.calledOnce(stub);
    });
    it("should make this.browserOpenNewtabStart() stop observing browser-open-newtab-start", async () => {
      await instance.init();
      sandbox.spy(Services.obs, "removeObserver");

      await instance.uninit();

      assert.calledOnce(Services.obs.removeObserver);
      assert.calledWithExactly(
        Services.obs.removeObserver,
        instance.browserOpenNewtabStart,
        "browser-open-newtab-start"
      );
    });
  });
  describe("#onAction", () => {
    beforeEach(() => {
      FAKE_GLOBAL_PREFS.clear();
    });
    it("should call .init() on an INIT action", () => {
      const init = sandbox.stub(instance, "init");
      const sendPageTakeoverData = sandbox.stub(
        instance,
        "sendPageTakeoverData"
      );

      instance.onAction({ type: at.INIT });

      assert.calledOnce(init);
      assert.calledOnce(sendPageTakeoverData);
    });
    it("should call .uninit() on an UNINIT action", () => {
      const stub = sandbox.stub(instance, "uninit");

      instance.onAction({ type: at.UNINIT });

      assert.calledOnce(stub);
    });
    it("should call .handleNewTabInit on a NEW_TAB_INIT action", () => {
      sandbox.spy(instance, "handleNewTabInit");

      instance.onAction(
        ac.AlsoToMain({
          type: at.NEW_TAB_INIT,
          data: { url: "about:newtab", browser },
        })
      );

      assert.calledOnce(instance.handleNewTabInit);
    });
    it("should call .addSession() on a NEW_TAB_INIT action", () => {
      const stub = sandbox.stub(instance, "addSession").returns({ perf: {} });
      sandbox.stub(instance, "setLoadTriggerInfo");

      instance.onAction(
        ac.AlsoToMain(
          {
            type: at.NEW_TAB_INIT,
            data: { url: "about:monkeys", browser },
          },
          "port123"
        )
      );

      assert.calledOnce(stub);
      assert.calledWith(stub, "port123", "about:monkeys");
    });
    it("should call .endSession() on a NEW_TAB_UNLOAD action", () => {
      const stub = sandbox.stub(instance, "endSession");

      instance.onAction(ac.AlsoToMain({ type: at.NEW_TAB_UNLOAD }, "port123"));

      assert.calledWith(stub, "port123");
    });
    it("should call .saveSessionPerfData on SAVE_SESSION_PERF_DATA", () => {
      const stub = sandbox.stub(instance, "saveSessionPerfData");
      const data = { some_ts: 10 };
      const action = { type: at.SAVE_SESSION_PERF_DATA, data };

      instance.onAction(ac.AlsoToMain(action, "port123"));

      assert.calledWith(stub, "port123", data);
    });
    it("should send an event on a TELEMETRY_USER_EVENT action", () => {
      FAKE_GLOBAL_PREFS.set(TELEMETRY_PREF, true);
      FAKE_GLOBAL_PREFS.set(EVENTS_TELEMETRY_PREF, true);
      instance = new TelemetryFeed();

      const utSendUserEvent = sandbox.stub(instance.utEvents, "sendUserEvent");
      const eventCreator = sandbox.stub(instance, "createUserEvent");

      const action = { type: at.TELEMETRY_USER_EVENT };

      instance.onAction(action);

      assert.calledWith(eventCreator, action);
      assert.calledWith(utSendUserEvent, eventCreator.returnValue);
    });
    it("should send an event on a DISCOVERY_STREAM_USER_EVENT action", () => {
      FAKE_GLOBAL_PREFS.set(TELEMETRY_PREF, true);
      FAKE_GLOBAL_PREFS.set(EVENTS_TELEMETRY_PREF, true);
      instance = new TelemetryFeed();

      const utSendUserEvent = sandbox.stub(instance.utEvents, "sendUserEvent");
      const eventCreator = sandbox.stub(instance, "createUserEvent");
      const action = { type: at.DISCOVERY_STREAM_USER_EVENT };

      instance.onAction(action);

      assert.calledWith(eventCreator, {
        ...action,
        data: {
          value: {
            pocket_logged_in_status: true,
          },
        },
      });
      assert.calledWith(utSendUserEvent, eventCreator.returnValue);
    });
    describe("should call handleASRouterUserEvent on x action", () => {
      const actions = [
        at.AS_ROUTER_TELEMETRY_USER_EVENT,
        msg.TOOLBAR_BADGE_TELEMETRY,
        msg.TOOLBAR_PANEL_TELEMETRY,
        msg.MOMENTS_PAGE_TELEMETRY,
        msg.DOORHANGER_TELEMETRY,
      ];
      actions.forEach(type => {
        it(`${type} action`, () => {
          FAKE_GLOBAL_PREFS.set(TELEMETRY_PREF, true);
          FAKE_GLOBAL_PREFS.set(EVENTS_TELEMETRY_PREF, true);
          instance = new TelemetryFeed();

          const eventHandler = sandbox.spy(instance, "handleASRouterUserEvent");
          const action = {
            type,
            data: { event: "CLICK" },
          };

          instance.onAction(action);

          assert.calledWith(eventHandler, action);
        });
      });
    });
    it("should call .handleDiscoveryStreamImpressionStats on a DISCOVERY_STREAM_IMPRESSION_STATS action", () => {
      const session = {};
      sandbox.stub(instance.sessions, "get").returns(session);
      const data = { source: "foo", tiles: [{ id: 1 }] };
      const action = { type: at.DISCOVERY_STREAM_IMPRESSION_STATS, data };
      sandbox.spy(instance, "handleDiscoveryStreamImpressionStats");

      instance.onAction(ac.AlsoToMain(action, "port123"));

      assert.calledWith(
        instance.handleDiscoveryStreamImpressionStats,
        "port123",
        data
      );
    });
    it("should call .handleTopSitesSponsoredImpressionStats on a TOP_SITES_SPONSORED_IMPRESSION_STATS action", () => {
      const session = {};
      sandbox.stub(instance.sessions, "get").returns(session);
      const data = { type: "impression", tile_id: 42, position: 1 };
      const action = { type: at.TOP_SITES_SPONSORED_IMPRESSION_STATS, data };
      sandbox.spy(instance, "handleTopSitesSponsoredImpressionStats");

      instance.onAction(ac.AlsoToMain(action));

      assert.calledOnce(instance.handleTopSitesSponsoredImpressionStats);
      assert.deepEqual(
        instance.handleTopSitesSponsoredImpressionStats.firstCall.args[0].data,
        data
      );
    });
    it("should call .handleAboutSponsoredTopSites on a ABOUT_SPONSORED_TOP_SITES action", () => {
      const data = { position: 0, advertiser_name: "moo", tile_id: 42 };
      const action = { type: at.ABOUT_SPONSORED_TOP_SITES, data };
      sandbox.spy(instance, "handleAboutSponsoredTopSites");

      instance.onAction(ac.AlsoToMain(action));

      assert.calledOnce(instance.handleAboutSponsoredTopSites);
    });
    it("should call #handleBlockUrl on a BLOCK_URL action", () => {
      const data = { position: 0, advertiser_name: "moo", tile_id: 42 };
      const action = { type: at.BLOCK_URL, data };
      sandbox.spy(instance, "handleBlockUrl");

      instance.onAction(ac.AlsoToMain(action));

      assert.calledOnce(instance.handleBlockUrl);
    });
  });
  it("should call .handleTopSitesOrganicImpressionStats on a TOP_SITES_ORGANIC_IMPRESSION_STATS action", () => {
    const session = {};
    sandbox.stub(instance.sessions, "get").returns(session);
    const data = { type: "impression", position: 1 };
    const action = { type: at.TOP_SITES_ORGANIC_IMPRESSION_STATS, data };
    sandbox.spy(instance, "handleTopSitesOrganicImpressionStats");

    instance.onAction(ac.AlsoToMain(action));

    assert.calledOnce(instance.handleTopSitesOrganicImpressionStats);
    assert.deepEqual(
      instance.handleTopSitesOrganicImpressionStats.firstCall.args[0].data,
      data
    );
  });
  describe("#handleNewTabInit", () => {
    it("should set the session as preloaded if the browser is preloaded", () => {
      const session = { perf: {} };
      let preloadedBrowser = {
        getAttribute() {
          return "preloaded";
        },
      };
      sandbox.stub(instance, "addSession").returns(session);

      instance.onAction(
        ac.AlsoToMain({
          type: at.NEW_TAB_INIT,
          data: { url: "about:newtab", browser: preloadedBrowser },
        })
      );

      assert.ok(session.perf.is_preloaded);
    });
    it("should set the session as non-preloaded if the browser is non-preloaded", () => {
      const session = { perf: {} };
      let nonPreloadedBrowser = {
        getAttribute() {
          return "";
        },
      };
      sandbox.stub(instance, "addSession").returns(session);

      instance.onAction(
        ac.AlsoToMain({
          type: at.NEW_TAB_INIT,
          data: { url: "about:newtab", browser: nonPreloadedBrowser },
        })
      );

      assert.ok(!session.perf.is_preloaded);
    });
  });
  describe("#SendASRouterUndesiredEvent", () => {
    it("should call handleASRouterUserEvent", () => {
      let stub = sandbox.stub(instance, "handleASRouterUserEvent");

      instance.SendASRouterUndesiredEvent({ foo: "bar" });

      assert.calledOnce(stub);
      let [payload] = stub.firstCall.args;
      assert.propertyVal(payload.data, "action", "asrouter_undesired_event");
      assert.propertyVal(payload.data, "foo", "bar");
    });
  });
  describe("#sendPageTakeoverData", () => {
    let fakePrefs = { "browser.newtabpage.enabled": true };

    beforeEach(() => {
      globals.set(
        "Services",
        Object.assign({}, Services, {
          prefs: { getBoolPref: key => fakePrefs[key] },
        })
      );
      // Services.prefs = {getBoolPref: key => fakePrefs[key]};
      sandbox.spy(Glean.newtab.newtabCategory, "set");
      sandbox.spy(Glean.newtab.homepageCategory, "set");
    });
    it("should record appropriate homepage category for about:home set to custom URL", async () => {
      fakeHomePageUrl = "https://searchprovider.com";
      instance._prefs.set(TELEMETRY_PREF, true);
      instance._classifySite = () => Promise.resolve("other");

      await instance.sendPageTakeoverData();
      assert.calledOnce(Glean.newtab.homepageCategory.set);
      assert.calledWith(Glean.newtab.homepageCategory.set, "other");
    });
    it("should send correct newtab category for about:newtab set to custom URL", async () => {
      globals.set("AboutNewTab", {
        newTabURLOverridden: true,
        newTabURL: "https://searchprovider.com",
      });
      instance._prefs.set(TELEMETRY_PREF, true);
      instance._classifySite = () => Promise.resolve("other");

      await instance.sendPageTakeoverData();
      assert.calledOnce(Glean.newtab.newtabCategory.set);
      assert.calledWith(Glean.newtab.newtabCategory.set, "other");
    });
    it("should not set home|newtab category if neither about:{home,newtab} are set to custom URL", async () => {
      instance._prefs.set(TELEMETRY_PREF, true);

      await instance.sendPageTakeoverData();
      assert.calledOnce(Glean.newtab.newtabCategory.set);
      assert.calledOnce(Glean.newtab.homepageCategory.set);
      assert.calledWith(Glean.newtab.newtabCategory.set, "enabled");
      assert.calledWith(Glean.newtab.homepageCategory.set, "enabled");
    });
    it("should set correct home|newtab category when changed by extension", async () => {
      const ID = "{abc-foo-bar}";
      fakeExtensionSettingsStore.getSetting = () => ({ id: ID });
      instance._prefs.set(TELEMETRY_PREF, true);
      instance._classifySite = () => Promise.resolve("other");

      await instance.sendPageTakeoverData();
      assert.calledOnce(Glean.newtab.newtabCategory.set);
      assert.calledOnce(Glean.newtab.homepageCategory.set);
      assert.equal(Glean.newtab.newtabCategory.set.args[0], "extension");
      assert.equal(Glean.newtab.homepageCategory.set.args[0], "extension");
    });
    it("instruments when newtab is disabled", async () => {
      instance._prefs.set(TELEMETRY_PREF, true);
      fakePrefs["browser.newtabpage.enabled"] = false;
      await instance.sendPageTakeoverData();
      assert.calledOnce(Glean.newtab.newtabCategory.set);
      assert.calledWith(Glean.newtab.newtabCategory.set, "disabled");
    });
    it("instruments when homepage is disabled", async () => {
      instance._prefs.set(TELEMETRY_PREF, true);
      fakeHomePage.overridden = true;
      await instance.sendPageTakeoverData();
      assert.calledOnce(Glean.newtab.homepageCategory.set);
      assert.calledWith(Glean.newtab.homepageCategory.set, "disabled");
    });
    it("should send a 'newtab' ping", async () => {
      instance._prefs.set(TELEMETRY_PREF, true);
      sandbox.spy(GleanPings.newtab, "submit");
      await instance.sendPageTakeoverData();
      assert.calledOnce(GleanPings.newtab.submit);
      assert.calledWithExactly(GleanPings.newtab.submit, "component_init");
    });
  });
  describe("#handleDiscoveryStreamImpressionStats", () => {
    it("should throw for a missing session", () => {
      assert.throws(() => {
        instance.handleDiscoveryStreamImpressionStats("a_missing_port", {});
      }, "Session does not exist.");
    });
    it("should instrument pocket impressions", () => {
      const session_id = "1337cafe";
      const pos1 = 1;
      const pos2 = 4;
      const shim = "Y29uc2lkZXIgeW91ciBjdXJpb3NpdHkgcmV3YXJkZWQ=";
      sandbox.stub(instance.sessions, "get").returns({ session_id });
      sandbox.spy(Glean.pocket.impression, "record");
      sandbox.spy(Glean.pocket.shim, "set");
      sandbox.spy(GleanPings.spoc, "submit");

      instance.handleDiscoveryStreamImpressionStats("_", {
        source: "foo",
        tiles: [
          {
            id: 1,
            pos: pos1,
            type: "organic",
            recommendation_id: "decaf-c0ff33",
          },
          {
            id: 2,
            pos: pos2,
            type: "spoc",
            recommendation_id: undefined,
            shim,
          },
        ],
        window_inner_width: 1000,
        window_inner_height: 900,
      });

      assert.calledTwice(Glean.pocket.impression.record);
      assert.deepEqual(Glean.pocket.impression.record.firstCall.args[0], {
        newtab_visit_id: session_id,
        is_sponsored: false,
        position: pos1,
        recommendation_id: "decaf-c0ff33",
        tile_id: 1,
      });
      assert.deepEqual(Glean.pocket.impression.record.secondCall.args[0], {
        newtab_visit_id: session_id,
        is_sponsored: true,
        position: pos2,
        recommendation_id: undefined,
        tile_id: 2,
      });
      assert.calledOnce(Glean.pocket.shim.set);
      assert.calledWith(Glean.pocket.shim.set, shim);
      assert.calledOnce(GleanPings.spoc.submit);
      assert.calledWith(GleanPings.spoc.submit, "impression");
    });
  });
  describe("#handleASRouterUserEvent", () => {
    it("should call submitGleanPingForPing on known pingTypes when telemetry is enabled", async () => {
      const data = {
        action: "onboarding_user_event",
        event: "IMPRESSION",
        message_id: "12345",
      };
      instance = new TelemetryFeed();
      instance._prefs.set(TELEMETRY_PREF, true);
      sandbox.spy(
        global.AboutWelcomeTelemetry.prototype,
        "submitGleanPingForPing"
      );

      await instance.handleASRouterUserEvent({ data });

      assert.calledOnce(
        global.AboutWelcomeTelemetry.prototype.submitGleanPingForPing
      );
    });
    it("should console.error and not submit pings on unknown pingTypes", async () => {
      const data = {
        action: "unknown_event",
        event: "IMPRESSION",
        message_id: "12345",
      };
      instance = new TelemetryFeed();
      instance._prefs.set(TELEMETRY_PREF, true);

      sandbox.spy(
        global.AboutWelcomeTelemetry.prototype,
        "submitGleanPingForPing"
      );
      await instance.handleASRouterUserEvent({ data });

      assert.calledOnce(global.console.error);
      assert.notCalled(
        global.AboutWelcomeTelemetry.prototype.submitGleanPingForPing
      );
    });
  });
  describe("#isInCFRCohort", () => {
    it("should return false if there is no CFR experiment registered", () => {
      assert.ok(!instance.isInCFRCohort);
    });
    it("should return true if there is a CFR experiment registered", () => {
      sandbox.stub(ExperimentAPI, "getExperimentMetaData").returns({
        slug: "SOME-CFR-EXP",
      });

      assert.ok(instance.isInCFRCohort);
      assert.propertyVal(
        ExperimentAPI.getExperimentMetaData.firstCall.args[0],
        "featureId",
        "cfr"
      );
    });
  });
  describe("#handleTopSitesSponsoredImpressionStats", () => {
    it("should add to keyed scalar on an impression event", async () => {
      const data = {
        type: "impression",
        tile_id: 42,
        source: "newtab",
        position: 0,
        reporting_url: "https://test.reporting.net/",
      };
      instance = new TelemetryFeed();
      sandbox.spy(Services.telemetry, "keyedScalarAdd");

      await instance.handleTopSitesSponsoredImpressionStats({ data });

      // Scalar should be added
      assert.calledOnce(Services.telemetry.keyedScalarAdd);
      assert.calledWith(
        Services.telemetry.keyedScalarAdd,
        "contextual.services.topsites.impression",
        "newtab_1",
        1
      );
    });
    it("should add to keyed scalar on a click event", async () => {
      const data = {
        type: "click",
        tile_id: 42,
        source: "newtab",
        position: 0,
        reporting_url: "https://test.reporting.net/",
      };
      instance = new TelemetryFeed();
      sandbox.spy(Services.telemetry, "keyedScalarAdd");

      await instance.handleTopSitesSponsoredImpressionStats({ data });

      // Scalar should be added
      assert.calledOnce(Services.telemetry.keyedScalarAdd);
      assert.calledWith(
        Services.telemetry.keyedScalarAdd,
        "contextual.services.topsites.click",
        "newtab_1",
        1
      );
    });
    it("should record a Glean topsites.impression event on an impression event", async () => {
      const data = {
        type: "impression",
        tile_id: 42,
        source: "newtab",
        position: 1,
        reporting_url: "https://test.reporting.net/",
        advertiser: "adnoid ads",
      };
      instance = new TelemetryFeed();
      const session_id = "decafc0ffee";
      sandbox.stub(instance.sessions, "get").returns({ session_id });
      sandbox.spy(Glean.topsites.impression, "record");

      await instance.handleTopSitesSponsoredImpressionStats({ data });

      // Event should be recorded
      assert.calledOnce(Glean.topsites.impression.record);
      assert.calledWith(Glean.topsites.impression.record, {
        advertiser_name: "adnoid ads",
        tile_id: data.tile_id,
        newtab_visit_id: session_id,
        is_sponsored: true,
        position: 1,
      });
    });
    it("should record a Glean topsites.click event on a click event", async () => {
      const data = {
        type: "click",
        advertiser: "test advertiser",
        tile_id: 42,
        source: "newtab",
        position: 0,
        reporting_url: "https://test.reporting.net/",
      };
      instance = new TelemetryFeed();
      const session_id = "decafc0ffee";
      sandbox.stub(instance.sessions, "get").returns({ session_id });
      sandbox.spy(Glean.topsites.click, "record");

      await instance.handleTopSitesSponsoredImpressionStats({ data });

      // Event should be recorded
      assert.calledOnce(Glean.topsites.click.record);
      assert.calledWith(Glean.topsites.click.record, {
        advertiser_name: "test advertiser",
        tile_id: data.tile_id,
        newtab_visit_id: session_id,
        is_sponsored: true,
        position: 0,
      });
    });
    it("should console.error on unknown pingTypes", async () => {
      const data = { type: "unknown_type" };
      instance = new TelemetryFeed();
      sandbox.spy(Glean.topsites.impression, "record");

      await instance.handleTopSitesSponsoredImpressionStats({ data });

      assert.calledOnce(global.console.error);
      assert.notCalled(Glean.topsites.impression.record);
    });
  });
  describe("#handleTopSitesOrganicImpressionStats", () => {
    it("should record a Glean topsites.impression event on an impression event", async () => {
      const data = {
        type: "impression",
        source: "newtab",
        position: 0,
      };
      instance = new TelemetryFeed();
      const session_id = "decafc0ffee";
      sandbox.stub(instance.sessions, "get").returns({ session_id });
      sandbox.spy(Glean.topsites.impression, "record");

      await instance.handleTopSitesOrganicImpressionStats({ data });

      assert.calledOnce(Glean.topsites.impression.record);
      assert.calledWith(Glean.topsites.impression.record, {
        newtab_visit_id: session_id,
        is_sponsored: false,
        position: 0,
      });
    });
    it("should record a Glean topsites.click event on a click event", async () => {
      const data = {
        type: "click",
        source: "newtab",
        position: 0,
      };
      instance = new TelemetryFeed();
      const session_id = "decafc0ffee";
      sandbox.stub(instance.sessions, "get").returns({ session_id });
      sandbox.spy(Glean.topsites.click, "record");

      await instance.handleTopSitesOrganicImpressionStats({ data });

      assert.calledOnce(Glean.topsites.click.record);
      assert.calledWith(Glean.topsites.click.record, {
        newtab_visit_id: session_id,
        is_sponsored: false,
        position: 0,
      });
    });
    it("should not record events on an unknown session", async () => {
      instance = new TelemetryFeed();
      sandbox.stub(instance.sessions, "get").returns(false);
      sandbox.spy(Glean.topsites.click, "record");
      sandbox.spy(Glean.topsites.impression, "record");

      await instance.handleTopSitesOrganicImpressionStats({});

      assert.notCalled(Glean.topsites.click.record);
      assert.notCalled(Glean.topsites.impression.record);
    });
    it("should record events on an unknown impressionStats action", async () => {
      const data = {
        type: "unknown",
      };
      instance = new TelemetryFeed();
      const session_id = "decafc0ffee";
      sandbox.stub(instance.sessions, "get").returns({ session_id });
      sandbox.spy(Glean.topsites.click, "record");
      sandbox.spy(Glean.topsites.impression, "record");

      await instance.handleTopSitesOrganicImpressionStats({ data });

      assert.notCalled(Glean.topsites.click.record);
      assert.notCalled(Glean.topsites.impression.record);
    });
  });
  describe("#handleDiscoveryStreamUserEvent", () => {
    it("correctly handles action with no `data`", () => {
      const action = ac.DiscoveryStreamUserEvent();
      instance = new TelemetryFeed();
      const session_id = "c0ffee";
      sandbox.stub(instance.sessions, "get").returns({ session_id });
      sandbox.spy(Glean.pocket.topicClick, "record");
      sandbox.spy(Glean.pocket.click, "record");
      sandbox.spy(Glean.pocket.save, "record");

      instance.handleDiscoveryStreamUserEvent(action);

      assert.notCalled(Glean.pocket.topicClick.record);
      assert.notCalled(Glean.pocket.click.record);
      assert.notCalled(Glean.pocket.save.record);
    });
    it("correctly handles CLICK data with no value", () => {
      const action = ac.DiscoveryStreamUserEvent({
        event: "CLICK",
        source: "POPULAR_TOPICS",
      });
      instance = new TelemetryFeed();
      const session_id = "c0ffee";
      sandbox.stub(instance.sessions, "get").returns({ session_id });
      sandbox.spy(Glean.pocket.topicClick, "record");

      instance.handleDiscoveryStreamUserEvent(action);

      assert.calledOnce(Glean.pocket.topicClick.record);
      assert.calledWith(Glean.pocket.topicClick.record, {
        newtab_visit_id: session_id,
        topic: undefined,
      });
    });
    it("correctly handles non-POPULAR_TOPICS CLICK data with no value", () => {
      const action = ac.DiscoveryStreamUserEvent({
        event: "CLICK",
        source: "not-POPULAR_TOPICS",
      });
      instance = new TelemetryFeed();
      const session_id = "c0ffee";
      sandbox.stub(instance.sessions, "get").returns({ session_id });
      sandbox.spy(Glean.pocket.topicClick, "record");
      sandbox.spy(Glean.pocket.click, "record");
      sandbox.spy(Glean.pocket.save, "record");

      instance.handleDiscoveryStreamUserEvent(action);

      assert.notCalled(Glean.pocket.topicClick.record);
      assert.notCalled(Glean.pocket.click.record);
      assert.notCalled(Glean.pocket.save.record);
    });
    it("correctly handles CLICK data with non-POPULAR_TOPICS source", () => {
      const topic = "atopic";
      const action = ac.DiscoveryStreamUserEvent({
        event: "CLICK",
        source: "not-POPULAR_TOPICS",
        value: {
          card_type: "topics_widget",
          topic,
        },
      });
      instance = new TelemetryFeed();
      const session_id = "c0ffee";
      sandbox.stub(instance.sessions, "get").returns({ session_id });
      sandbox.spy(Glean.pocket.topicClick, "record");

      instance.handleDiscoveryStreamUserEvent(action);

      assert.calledOnce(Glean.pocket.topicClick.record);
      assert.calledWith(Glean.pocket.topicClick.record, {
        newtab_visit_id: session_id,
        topic,
      });
    });
    it("doesn't instrument a CLICK without a card_type", () => {
      const action = ac.DiscoveryStreamUserEvent({
        event: "CLICK",
        source: "not-POPULAR_TOPICS",
        value: {
          card_type: "not spoc, organic, or topics_widget",
        },
      });
      instance = new TelemetryFeed();
      const session_id = "c0ffee";
      sandbox.stub(instance.sessions, "get").returns({ session_id });
      sandbox.spy(Glean.pocket.topicClick, "record");
      sandbox.spy(Glean.pocket.click, "record");
      sandbox.spy(Glean.pocket.save, "record");

      instance.handleDiscoveryStreamUserEvent(action);

      assert.notCalled(Glean.pocket.topicClick.record);
      assert.notCalled(Glean.pocket.click.record);
      assert.notCalled(Glean.pocket.save.record);
    });
    it("instruments a popular topic click", () => {
      const topic = "entertainment";
      const action = ac.DiscoveryStreamUserEvent({
        event: "CLICK",
        source: "POPULAR_TOPICS",
        value: {
          card_type: "topics_widget",
          topic,
        },
      });
      instance = new TelemetryFeed();
      const session_id = "c0ffee";
      sandbox.stub(instance.sessions, "get").returns({ session_id });
      sandbox.spy(Glean.pocket.topicClick, "record");

      instance.handleDiscoveryStreamUserEvent(action);

      assert.calledOnce(Glean.pocket.topicClick.record);
      assert.calledWith(Glean.pocket.topicClick.record, {
        newtab_visit_id: session_id,
        topic,
      });
    });
    it("instruments an organic top stories click", () => {
      const action_position = 42;
      const action = ac.DiscoveryStreamUserEvent({
        event: "CLICK",
        action_position,
        value: {
          card_type: "organic",
          recommendation_id: "decaf-c0ff33",
          tile_id: 314623757745896,
        },
      });
      instance = new TelemetryFeed();
      const session_id = "c0ffee";
      sandbox.stub(instance.sessions, "get").returns({ session_id });
      sandbox.spy(Glean.pocket.click, "record");
      sandbox.spy(Glean.pocket.shim, "set");
      sandbox.spy(GleanPings.spoc, "submit");

      instance.handleDiscoveryStreamUserEvent(action);

      assert.calledOnce(Glean.pocket.click.record);
      assert.calledWith(Glean.pocket.click.record, {
        newtab_visit_id: session_id,
        is_sponsored: false,
        position: action_position,
        recommendation_id: "decaf-c0ff33",
        tile_id: 314623757745896,
      });
      assert.notCalled(Glean.pocket.shim.set);
      assert.notCalled(GleanPings.spoc.submit);
    });
    it("instruments a sponsored top stories click", () => {
      const action_position = 42;
      const shim = "Y29uc2lkZXIgeW91ciBjdXJpb3NpdHkgcmV3YXJkZWQ=";
      const action = ac.DiscoveryStreamUserEvent({
        event: "CLICK",
        action_position,
        value: {
          card_type: "spoc",
          recommendation_id: undefined,
          tile_id: 448685088,
          shim,
        },
      });
      instance = new TelemetryFeed();
      const session_id = "c0ffee";
      sandbox.stub(instance.sessions, "get").returns({ session_id });
      sandbox.spy(Glean.pocket.click, "record");
      sandbox.spy(Glean.pocket.shim, "set");
      sandbox.spy(GleanPings.spoc, "submit");

      instance.handleDiscoveryStreamUserEvent(action);

      assert.calledOnce(Glean.pocket.click.record);
      assert.calledWith(Glean.pocket.click.record, {
        newtab_visit_id: session_id,
        is_sponsored: true,
        position: action_position,
        recommendation_id: undefined,
        tile_id: 448685088,
      });
      assert.calledOnce(Glean.pocket.shim.set);
      assert.calledWith(Glean.pocket.shim.set, shim);
      assert.calledOnce(GleanPings.spoc.submit);
      assert.calledWith(GleanPings.spoc.submit, "click");
    });
    it("instruments a save of an organic top story", () => {
      const action_position = 42;
      const action = ac.DiscoveryStreamUserEvent({
        event: "SAVE_TO_POCKET",
        action_position,
        value: {
          card_type: "organic",
          recommendation_id: "decaf-c0ff33",
          tile_id: 314623757745896,
        },
      });
      instance = new TelemetryFeed();
      const session_id = "c0ffee";
      sandbox.stub(instance.sessions, "get").returns({ session_id });
      sandbox.spy(Glean.pocket.save, "record");
      sandbox.spy(Glean.pocket.shim, "set");
      sandbox.spy(GleanPings.spoc, "submit");

      instance.handleDiscoveryStreamUserEvent(action);

      assert.calledOnce(Glean.pocket.save.record);
      assert.calledWith(Glean.pocket.save.record, {
        newtab_visit_id: session_id,
        is_sponsored: false,
        position: action_position,
        recommendation_id: "decaf-c0ff33",
        tile_id: 314623757745896,
      });
      assert.notCalled(Glean.pocket.shim.set);
      assert.notCalled(GleanPings.spoc.submit);
    });
    it("instruments a save of a sponsored top story", () => {
      const action_position = 42;
      const shim = "Y29uc2lkZXIgeW91ciBjdXJpb3NpdHkgcmV3YXJkZWQ=";
      const action = ac.DiscoveryStreamUserEvent({
        event: "SAVE_TO_POCKET",
        action_position,
        value: {
          card_type: "spoc",
          recommendation_id: undefined,
          tile_id: 448685088,
          shim,
        },
      });
      instance = new TelemetryFeed();
      const session_id = "c0ffee";
      sandbox.stub(instance.sessions, "get").returns({ session_id });
      sandbox.spy(Glean.pocket.save, "record");
      sandbox.spy(Glean.pocket.shim, "set");
      sandbox.spy(GleanPings.spoc, "submit");

      instance.handleDiscoveryStreamUserEvent(action);

      assert.calledOnce(Glean.pocket.save.record);
      assert.calledWith(Glean.pocket.save.record, {
        newtab_visit_id: session_id,
        is_sponsored: true,
        position: action_position,
        recommendation_id: undefined,
        tile_id: 448685088,
      });
      assert.calledOnce(Glean.pocket.shim.set);
      assert.calledWith(Glean.pocket.shim.set, shim);
      assert.calledOnce(GleanPings.spoc.submit);
      assert.calledWith(GleanPings.spoc.submit, "save");
    });
    it("instruments a save of a sponsored top story, without `value`", () => {
      const action_position = 42;
      const action = ac.DiscoveryStreamUserEvent({
        event: "SAVE_TO_POCKET",
        action_position,
      });
      instance = new TelemetryFeed();
      const session_id = "c0ffee";
      sandbox.stub(instance.sessions, "get").returns({ session_id });
      sandbox.spy(Glean.pocket.save, "record");
      sandbox.spy(Glean.pocket.shim, "set");
      sandbox.spy(GleanPings.spoc, "submit");

      instance.handleDiscoveryStreamUserEvent(action);

      assert.calledOnce(Glean.pocket.save.record);
      assert.calledWith(Glean.pocket.save.record, {
        newtab_visit_id: session_id,
        is_sponsored: false,
        position: action_position,
        recommendation_id: undefined,
        tile_id: undefined,
      });
      assert.notCalled(Glean.pocket.shim.set);
      assert.notCalled(GleanPings.spoc.submit);
    });
  });
  describe("#handleAboutSponsoredTopSites", () => {
    it("should record a Glean topsites.showPrivacyClick event on action", async () => {
      const data = {
        position: 42,
        advertiser_name: "mozilla",
        tile_id: 4567,
      };
      instance = new TelemetryFeed();
      const session_id = "decafc0ffee";
      sandbox.stub(instance.sessions, "get").returns({ session_id });
      sandbox.spy(Glean.topsites.showPrivacyClick, "record");

      await instance.handleAboutSponsoredTopSites({ data });

      assert.calledOnce(Glean.topsites.showPrivacyClick.record);
      assert.calledWith(Glean.topsites.showPrivacyClick.record, {
        advertiser_name: data.advertiser_name,
        tile_id: data.tile_id,
        newtab_visit_id: session_id,
        position: data.position,
      });
    });
    it("should not record a Glean topsites.showPrivacyClick event if there's no session", async () => {
      const data = {
        position: 42,
        advertiser_name: "mozilla",
        tile_id: 4567,
      };
      instance = new TelemetryFeed();
      sandbox.stub(instance.sessions, "get").returns(null);
      sandbox.spy(Glean.topsites.showPrivacyClick, "record");

      await instance.handleAboutSponsoredTopSites({ data });

      assert.notCalled(Glean.topsites.showPrivacyClick.record);
    });
  });
  describe("#handleBlockUrl", () => {
    it("shouldn't record events for pocket cards' dismisses", async () => {
      const data = [
        {
          // Shouldn't record anything for this one
          is_pocket_card: true,
          position: 43,
          tile_id: undefined,
        },
      ];
      instance = new TelemetryFeed();
      const session_id = "decafc0ffee";
      sandbox.stub(instance.sessions, "get").returns({ session_id });
      sandbox.spy(Glean.topsites.dismiss, "record");

      await instance.handleBlockUrl({ data });

      assert.notCalled(Glean.topsites.dismiss.record);
    });
    it("should record a Glean topsites.dismiss event on action", async () => {
      const data = [
        {
          is_pocket_card: false,
          position: 42,
          advertiser_name: "mozilla",
          tile_id: 4567,
          isSponsoredTopSite: 1, // for some reason this is an int.
        },
      ];
      instance = new TelemetryFeed();
      const session_id = "decafc0ffee";
      sandbox.stub(instance.sessions, "get").returns({ session_id });
      sandbox.spy(Glean.topsites.dismiss, "record");

      await instance.handleBlockUrl({ data });

      assert.calledOnce(Glean.topsites.dismiss.record);
      assert.calledWith(Glean.topsites.dismiss.record, {
        advertiser_name: data[0].advertiser_name,
        tile_id: data[0].tile_id,
        newtab_visit_id: session_id,
        is_sponsored: !!data[0].isSponsoredTopSite,
        position: data[0].position,
      });
    });
    it("should record a Glean topsites.dismiss event on action on non-sponsored topsite", async () => {
      const data = [
        {
          is_pocket_card: false,
          position: 42,
          tile_id: undefined,
        },
      ];
      instance = new TelemetryFeed();
      const session_id = "decafc0ffee";
      sandbox.stub(instance.sessions, "get").returns({ session_id });
      sandbox.spy(Glean.topsites.dismiss, "record");

      await instance.handleBlockUrl({ data });

      assert.calledOnce(Glean.topsites.dismiss.record);
      assert.calledWith(Glean.topsites.dismiss.record, {
        advertiser_name: undefined,
        tile_id: undefined,
        newtab_visit_id: session_id,
        is_sponsored: false,
        position: data[0].position,
      });
    });
    it("should not record a Glean topsites.dismiss event if there's no session", async () => {
      const data = {};
      instance = new TelemetryFeed();
      sandbox.stub(instance.sessions, "get").returns(null);
      sandbox.spy(Glean.topsites.dismiss, "record");

      await instance.handleBlockUrl({ data });

      assert.notCalled(Glean.topsites.dismiss.record);
    });
  });
});
