/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const { actionTypes: at, actionUtils: au } = ChromeUtils.import(
  "resource://activity-stream/common/Actions.jsm"
);
const { Prefs } = ChromeUtils.import(
  "resource://activity-stream/lib/ActivityStreamPrefs.jsm"
);
const { classifySite } = ChromeUtils.import(
  "resource://activity-stream/lib/SiteClassifier.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "ASRouterPreferences",
  "resource://activity-stream/lib/ASRouterPreferences.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "perfService",
  "resource://activity-stream/common/PerfService.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "PingCentre",
  "resource:///modules/PingCentre.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "UTEventReporting",
  "resource://activity-stream/lib/UTEventReporting.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "UpdateUtils",
  "resource://gre/modules/UpdateUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "HomePage",
  "resource:///modules/HomePage.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "ExtensionSettingsStore",
  "resource://gre/modules/ExtensionSettingsStore.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm"
);

XPCOMUtils.defineLazyServiceGetters(this, {
  gUUIDGenerator: ["@mozilla.org/uuid-generator;1", "nsIUUIDGenerator"],
  aboutNewTabService: [
    "@mozilla.org/browser/aboutnewtab-service;1",
    "nsIAboutNewTabService",
  ],
});

const ACTIVITY_STREAM_ID = "activity-stream";
const ACTIVITY_STREAM_ENDPOINT_PREF =
  "browser.newtabpage.activity-stream.telemetry.ping.endpoint";
const ACTIVITY_STREAM_ROUTER_ID = "activity-stream-router";
const DOMWINDOW_OPENED_TOPIC = "domwindowopened";
const DOMWINDOW_UNLOAD_TOPIC = "unload";
const TAB_PINNED_EVENT = "TabPinned";

// This is a mapping table between the user preferences and its encoding code
const USER_PREFS_ENCODING = {
  showSearch: 1 << 0,
  "feeds.topsites": 1 << 1,
  "feeds.section.topstories": 1 << 2,
  "feeds.section.highlights": 1 << 3,
  "feeds.snippets": 1 << 4,
  showSponsored: 1 << 5,
  "asrouter.userprefs.cfr.addons": 1 << 6,
  "asrouter.userprefs.cfr.features": 1 << 7,
};

const PREF_IMPRESSION_ID = "impressionId";
const TELEMETRY_PREF = "telemetry";
const EVENTS_TELEMETRY_PREF = "telemetry.ut.events";
const STRUCTURED_INGESTION_TELEMETRY_PREF = "telemetry.structuredIngestion";
const STRUCTURED_INGESTION_ENDPOINT_PREF =
  "telemetry.structuredIngestion.endpoint";

this.TelemetryFeed = class TelemetryFeed {
  constructor(options) {
    this.sessions = new Map();
    this._prefs = new Prefs();
    this._impressionId = this.getOrCreateImpressionId();
    this._aboutHomeSeen = false;
    this._classifySite = classifySite;
    this._addWindowListeners = this._addWindowListeners.bind(this);
    this.handleEvent = this.handleEvent.bind(this);
  }

  get telemetryEnabled() {
    return this._prefs.get(TELEMETRY_PREF);
  }

  get eventTelemetryEnabled() {
    return this._prefs.get(EVENTS_TELEMETRY_PREF);
  }

  get structuredIngestionTelemetryEnabled() {
    return this._prefs.get(STRUCTURED_INGESTION_TELEMETRY_PREF);
  }

  get structuredIngestionEndpointBase() {
    return this._prefs.get(STRUCTURED_INGESTION_ENDPOINT_PREF);
  }

  init() {
    Services.obs.addObserver(
      this.browserOpenNewtabStart,
      "browser-open-newtab-start"
    );
    // Add pin tab event listeners on future windows
    Services.obs.addObserver(this._addWindowListeners, DOMWINDOW_OPENED_TOPIC);
    // Listen for pin tab events on all open windows
    for (let win of Services.wm.getEnumerator("navigator:browser")) {
      this._addWindowListeners(win);
    }
  }

  handleEvent(event) {
    switch (event.type) {
      case TAB_PINNED_EVENT:
        this.countPinnedTab(event.target);
        break;
      case DOMWINDOW_UNLOAD_TOPIC:
        this._removeWindowListeners(event.target);
        break;
    }
  }

  _removeWindowListeners(win) {
    win.removeEventListener(DOMWINDOW_UNLOAD_TOPIC, this.handleEvent);
    win.removeEventListener(TAB_PINNED_EVENT, this.handleEvent);
  }

  _addWindowListeners(win) {
    win.addEventListener(DOMWINDOW_UNLOAD_TOPIC, this.handleEvent);
    win.addEventListener(TAB_PINNED_EVENT, this.handleEvent);
  }

  countPinnedTab(target, source = "TAB_CONTEXT_MENU") {
    const win = target.ownerGlobal;
    if (PrivateBrowsingUtils.isWindowPrivate(win)) {
      return;
    }
    const event = Object.assign(this.createPing(), {
      action: "activity_stream_user_event",
      event: TAB_PINNED_EVENT.toUpperCase(),
      value: { total_pinned_tabs: this.countTotalPinnedTabs() },
      source,
      // These fields are required but not relevant for this ping
      page: "n/a",
      session_id: "n/a",
    });
    this.sendEvent(event);
  }

  countTotalPinnedTabs() {
    let pinnedTabs = 0;
    for (let win of Services.wm.getEnumerator("navigator:browser")) {
      if (win.closed || PrivateBrowsingUtils.isWindowPrivate(win)) {
        continue;
      }
      for (let tab of win.gBrowser.tabs) {
        pinnedTabs += tab.pinned ? 1 : 0;
      }
    }

    return pinnedTabs;
  }

  getOrCreateImpressionId() {
    let impressionId = this._prefs.get(PREF_IMPRESSION_ID);
    if (!impressionId) {
      impressionId = String(gUUIDGenerator.generateUUID());
      this._prefs.set(PREF_IMPRESSION_ID, impressionId);
    }
    return impressionId;
  }

  browserOpenNewtabStart() {
    perfService.mark("browser-open-newtab-start");
  }

  setLoadTriggerInfo(port) {
    // XXX note that there is a race condition here; we're assuming that no
    // other tab will be interleaving calls to browserOpenNewtabStart and
    // when at.NEW_TAB_INIT gets triggered by RemotePages and calls this
    // method.  For manually created windows, it's hard to imagine us hitting
    // this race condition.
    //
    // However, for session restore, where multiple windows with multiple tabs
    // might be restored much closer together in time, it's somewhat less hard,
    // though it should still be pretty rare.
    //
    // The fix to this would be making all of the load-trigger notifications
    // return some data with their notifications, and somehow propagate that
    // data through closures into the tab itself so that we could match them
    //
    // As of this writing (very early days of system add-on perf telemetry),
    // the hypothesis is that hitting this race should be so rare that makes
    // more sense to live with the slight data inaccuracy that it would
    // introduce, rather than doing the correct but complicated thing.  It may
    // well be worth reexamining this hypothesis after we have more experience
    // with the data.

    let data_to_save;
    try {
      data_to_save = {
        load_trigger_ts: perfService.getMostRecentAbsMarkStartByName(
          "browser-open-newtab-start"
        ),
        load_trigger_type: "menu_plus_or_keyboard",
      };
    } catch (e) {
      // if no mark was returned, we have nothing to save
      return;
    }
    this.saveSessionPerfData(port, data_to_save);
  }

  /**
   * Lazily initialize PingCentre for Activity Stream to send pings
   */
  get pingCentre() {
    Object.defineProperty(this, "pingCentre", {
      value: new PingCentre({
        topic: ACTIVITY_STREAM_ID,
        overrideEndpointPref: ACTIVITY_STREAM_ENDPOINT_PREF,
      }),
    });
    return this.pingCentre;
  }

  /**
   * Lazily initialize a PingCentre client for Activity Stream Router to send pings.
   *
   * Unlike the PingCentre client for Activity Stream, Activity Stream Router
   * uses a separate client with the standard PingCentre endpoint.
   */
  get pingCentreForASRouter() {
    Object.defineProperty(this, "pingCentreForASRouter", {
      value: new PingCentre({ topic: ACTIVITY_STREAM_ROUTER_ID }),
    });
    return this.pingCentreForASRouter;
  }

  /**
   * Lazily initialize UTEventReporting to send pings
   */
  get utEvents() {
    Object.defineProperty(this, "utEvents", { value: new UTEventReporting() });
    return this.utEvents;
  }

  /**
   * Get encoded user preferences, multiple prefs will be combined via bitwise OR operator
   */
  get userPreferences() {
    let prefs = 0;

    for (const pref of Object.keys(USER_PREFS_ENCODING)) {
      if (this._prefs.get(pref)) {
        prefs |= USER_PREFS_ENCODING[pref];
      }
    }
    return prefs;
  }

  /**
   *  Check if it is in the CFR experiment cohort. ASRouterPreferences lazily parses AS router pref.
   */
  get isInCFRCohort() {
    for (let provider of ASRouterPreferences.providers) {
      if (provider.id === "cfr" && provider.enabled && provider.cohort) {
        return true;
      }
    }
    return false;
  }

  /**
   * addSession - Start tracking a new session
   *
   * @param  {string} id the portID of the open session
   * @param  {string} the URL being loaded for this session (optional)
   * @return {obj}    Session object
   */
  addSession(id, url) {
    // XXX refactor to use setLoadTriggerInfo or saveSessionPerfData

    // "unexpected" will be overwritten when appropriate
    let load_trigger_type = "unexpected";
    let load_trigger_ts;

    if (!this._aboutHomeSeen && url === "about:home") {
      this._aboutHomeSeen = true;

      // XXX note that this will be incorrectly set in the following cases:
      // session_restore following by clicking on the toolbar button,
      // or someone who has changed their default home page preference to
      // something else and later clicks the toolbar.  It will also be
      // incorrectly unset if someone changes their "Home Page" preference to
      // about:newtab.
      //
      // That said, the ratio of these mistakes to correct cases should
      // be very small, and these issues should follow away as we implement
      // the remaining load_trigger_type values for about:home in issue 3556.
      //
      // XXX file a bug to implement remaining about:home cases so this
      // problem will go away and link to it here.
      load_trigger_type = "first_window_opened";

      // The real perceived trigger of first_window_opened is the OS-level
      // clicking of the icon.  We use perfService.timeOrigin because it's the
      // earliest number on this time scale that's easy to get.; We could
      // actually use 0, but maybe that could be before the browser started?
      // [bug 1401406](https://bugzilla.mozilla.org/show_bug.cgi?id=1401406)
      // getting sorted out may help clarify. Even better, presumably, would be
      // to use the process creation time for the main process, which is
      // available, but somewhat harder to get. However, these are all more or
      // less proxies for the same thing, so it's not clear how much the better
      // numbers really matter, since we (activity stream) only control a
      // relatively small amount of the code that's executing between the
      // OS-click and when the first <browser> element starts loading.  That
      // said, it's conceivable that it could help us catch regressions in the
      // number of cycles early chrome code takes to execute, but it's likely
      // that there are more direct ways to measure that.
      load_trigger_ts = perfService.timeOrigin;
    }

    const session = {
      session_id: String(gUUIDGenerator.generateUUID()),
      // "unknown" will be overwritten when appropriate
      page: url ? url : "unknown",
      perf: {
        load_trigger_type,
        is_preloaded: false,
      },
    };

    if (load_trigger_ts) {
      session.perf.load_trigger_ts = load_trigger_ts;
    }

    this.sessions.set(id, session);
    return session;
  }

  /**
   * endSession - Stop tracking a session
   *
   * @param  {string} portID the portID of the session that just closed
   */
  endSession(portID) {
    const session = this.sessions.get(portID);

    if (!session) {
      // It's possible the tab was never visible – in which case, there was no user session.
      return;
    }

    this.sendDiscoveryStreamLoadedContent(portID, session);
    this.sendDiscoveryStreamImpressions(portID, session);

    if (session.perf.visibility_event_rcvd_ts) {
      session.session_duration = Math.round(
        perfService.absNow() - session.perf.visibility_event_rcvd_ts
      );
    } else {
      // This session was never shown (i.e. the hidden preloaded newtab), there was no user session either.
      this.sessions.delete(portID);
      return;
    }

    let sessionEndEvent = this.createSessionEndEvent(session);
    this.sendEvent(sessionEndEvent);
    this.sendUTEvent(sessionEndEvent, this.utEvents.sendSessionEndEvent);
    this.sessions.delete(portID);
  }

  /**
   * Send impression pings for Discovery Stream for a given session.
   *
   * @note the impression reports are stored in session.impressionSets for different
   * sources, and will be sent separately accordingly.
   *
   * @param {String} port  The session port with which this is associated
   * @param {Object} session  The session object
   */
  sendDiscoveryStreamImpressions(port, session) {
    const { impressionSets } = session;

    if (!impressionSets) {
      return;
    }

    Object.keys(impressionSets).forEach(source => {
      const payload = this.createImpressionStats(port, {
        source,
        tiles: impressionSets[source],
      });
      this.sendEvent(payload);
      this.sendStructuredIngestionEvent(payload, "impression-stats", "1");
    });
  }

  /**
   * Send loaded content pings for Discovery Stream for a given session.
   *
   * @note the loaded content reports are stored in session.loadedContentSets for different
   * sources, and will be sent separately accordingly.
   *
   * @param {String} port  The session port with which this is associated
   * @param {Object} session  The session object
   */
  sendDiscoveryStreamLoadedContent(port, session) {
    const { loadedContentSets } = session;

    if (!loadedContentSets) {
      return;
    }

    Object.keys(loadedContentSets).forEach(source => {
      const tiles = loadedContentSets[source];
      const payload = this.createImpressionStats(port, {
        source,
        tiles,
        loaded: tiles.length,
      });
      this.sendEvent(payload);
      this.sendStructuredIngestionEvent(payload, "impression-stats", "1");
    });
  }

  /**
   * handleNewTabInit - Handle NEW_TAB_INIT, which creates a new session and sets the a flag
   *                    for session.perf based on whether or not this new tab is preloaded
   *
   * @param  {obj} action the Action object
   */
  handleNewTabInit(action) {
    const session = this.addSession(
      au.getPortIdOfSender(action),
      action.data.url
    );
    session.perf.is_preloaded =
      action.data.browser.getAttribute("preloadedState") === "preloaded";
  }

  /**
   * createPing - Create a ping with common properties
   *
   * @param  {string} id The portID of the session, if a session is relevant (optional)
   * @return {obj}    A telemetry ping
   */
  createPing(portID) {
    const ping = {
      addon_version: Services.appinfo.appBuildID,
      locale: Services.locale.appLocaleAsLangTag,
      user_prefs: this.userPreferences,
    };

    // If the ping is part of a user session, add session-related info
    if (portID) {
      const session = this.sessions.get(portID) || this.addSession(portID);
      Object.assign(ping, { session_id: session.session_id });

      if (session.page) {
        Object.assign(ping, { page: session.page });
      }
    }
    return ping;
  }

  /**
   * createImpressionStats - Create a ping for an impression stats
   *
   * @param  {string} portID The portID of the open session
   * @param  {ob} data The data object to be included in the ping.
   * @return {obj}    A telemetry ping
   */
  createImpressionStats(portID, data) {
    return Object.assign(this.createPing(portID), data, {
      action: "activity_stream_impression_stats",
      impression_id: this._impressionId,
      client_id: "n/a",
      session_id: "n/a",
    });
  }

  createSpocsFillPing(data) {
    return Object.assign(this.createPing(null), data, {
      impression_id: this._impressionId,
      client_id: "n/a",
      session_id: "n/a",
    });
  }

  createUserEvent(action) {
    return Object.assign(
      this.createPing(au.getPortIdOfSender(action)),
      action.data,
      { action: "activity_stream_user_event" }
    );
  }

  createUndesiredEvent(action) {
    return Object.assign(
      this.createPing(au.getPortIdOfSender(action)),
      { value: 0 }, // Default value
      action.data,
      { action: "activity_stream_undesired_event" }
    );
  }

  createPerformanceEvent(action) {
    return Object.assign(this.createPing(), action.data, {
      action: "activity_stream_performance_event",
    });
  }

  createSessionEndEvent(session) {
    return Object.assign(this.createPing(), {
      session_id: session.session_id,
      page: session.page,
      session_duration: session.session_duration,
      action: "activity_stream_session",
      perf: session.perf,
    });
  }

  /**
   * Create a ping for AS router event. The client_id is set to "n/a" by default,
   * different component can override this by its own telemetry collection policy.
   */
  createASRouterEvent(action) {
    const ping = {
      client_id: "n/a",
      addon_version: Services.appinfo.appBuildID,
      locale: Services.locale.appLocaleAsLangTag,
      impression_id: this._impressionId,
    };
    const event = Object.assign(ping, action.data);
    if (event.action === "cfr_user_event") {
      return this.applyCFRPolicy(event);
    } else if (event.action === "snippets_user_event") {
      return this.applySnippetsPolicy(event);
    } else if (event.action === "onboarding_user_event") {
      return this.applyOnboardingPolicy(event);
    }
    return event;
  }

  /**
   * Per Bug 1484035, CFR metrics comply with following policies:
   * 1). In release, it collects impression_id, and treats bucket_id as message_id
   * 2). In prerelease, it collects client_id and message_id
   * 3). In shield experiments conducted in release, it collects client_id and message_id
   */
  applyCFRPolicy(ping) {
    if (
      UpdateUtils.getUpdateChannel(true) === "release" &&
      !this.isInCFRCohort
    ) {
      ping.message_id = ping.bucket_id || "n/a";
      ping.client_id = "n/a";
      ping.impression_id = this._impressionId;
    } else {
      ping.impression_id = "n/a";
      // Ping-centre client will fill in the client_id if it's not provided in the ping.
      delete ping.client_id;
    }
    // bucket_id is no longer needed
    delete ping.bucket_id;
    return ping;
  }

  /**
   * Per Bug 1485069, all the metrics for Snippets in AS router use client_id in
   * all the release channels
   */
  applySnippetsPolicy(ping) {
    // Ping-centre client will fill in the client_id if it's not provided in the ping.
    delete ping.client_id;
    ping.impression_id = "n/a";
    return ping;
  }

  /**
   * Per Bug 1482134, all the metrics for Onboarding in AS router use client_id in
   * all the release channels
   */
  applyOnboardingPolicy(ping) {
    // Ping-centre client will fill in the client_id if it's not provided in the ping.
    delete ping.client_id;
    ping.impression_id = "n/a";
    return ping;
  }

  sendEvent(event_object) {
    if (this.telemetryEnabled) {
      this.pingCentre.sendPing(event_object, { filter: ACTIVITY_STREAM_ID });
    }
  }

  sendUTEvent(event_object, eventFunction) {
    if (this.telemetryEnabled && this.eventTelemetryEnabled) {
      eventFunction(event_object);
    }
  }

  /**
   * Generates an endpoint for Structured Ingestion telemetry pipeline. Note that
   * Structured Ingestion requires a different endpoint for each ping. See more
   * details about endpoint schema at:
   * https://github.com/mozilla/gcp-ingestion/blob/master/docs/edge.md#postput-request
   *
   * @param {String} pingType  Type of the ping, such as "impression-stats".
   * @param {String} version   Endpoint version for this ping type.
   */
  _generateStructuredIngestionEndpoint(pingType, version) {
    const uuid = gUUIDGenerator.generateUUID().toString();
    // Structured Ingestion does not support the UUID generated by gUUIDGenerator,
    // because it contains leading and trailing braces. Need to trim them first.
    const docID = uuid.slice(1, -1);
    const extension = `${pingType}/${version}/${docID}`;
    return `${this.structuredIngestionEndpointBase}/${extension}`;
  }

  sendStructuredIngestionEvent(event_object, pingType, version) {
    if (this.telemetryEnabled && this.structuredIngestionTelemetryEnabled) {
      this.pingCentre.sendStructuredIngestionPing(
        event_object,
        this._generateStructuredIngestionEndpoint(pingType, version),
        { filter: ACTIVITY_STREAM_ID }
      );
    }
  }

  sendASRouterEvent(event_object) {
    if (this.telemetryEnabled) {
      this.pingCentreForASRouter.sendPing(event_object, {
        filter: ACTIVITY_STREAM_ID,
      });
    }
  }

  handleImpressionStats(action) {
    const payload = this.createImpressionStats(
      au.getPortIdOfSender(action),
      action.data
    );
    this.sendEvent(payload);
    this.sendStructuredIngestionEvent(payload, "impression-stats", "1");
  }

  handleUserEvent(action) {
    let userEvent = this.createUserEvent(action);
    this.sendEvent(userEvent);
    this.sendUTEvent(userEvent, this.utEvents.sendUserEvent);
  }

  handleASRouterUserEvent(action) {
    let event = this.createASRouterEvent(action);
    this.sendASRouterEvent(event);
  }

  handleUndesiredEvent(action) {
    this.sendEvent(this.createUndesiredEvent(action));
  }

  handleTrailheadEnrollEvent(action) {
    // Unlike `sendUTEvent`, we always send the event if AS's telemetry is enabled
    // regardless of `this.eventTelemetryEnabled`.
    if (this.telemetryEnabled) {
      this.utEvents.sendTrailheadEnrollEvent(action.data);
    }
  }

  async sendPageTakeoverData() {
    if (this.telemetryEnabled) {
      const value = {};
      let newtabAffected = false;
      let homeAffected = false;

      // Check whether or not about:home and about:newtab are set to a custom URL.
      // If so, classify them.
      if (
        Services.prefs.getBoolPref("browser.newtabpage.enabled") &&
        aboutNewTabService.overridden &&
        !aboutNewTabService.newTabURL.startsWith("moz-extension://")
      ) {
        value.newtab_url_category = await this._classifySite(
          aboutNewTabService.newTabURL
        );
        newtabAffected = true;
      }
      // Check if the newtab page setting is controlled by an extension.
      await ExtensionSettingsStore.initialize();
      const newtabExtensionInfo = ExtensionSettingsStore.getSetting(
        "url_overrides",
        "newTabURL"
      );
      if (newtabExtensionInfo && newtabExtensionInfo.id) {
        value.newtab_extension_id = newtabExtensionInfo.id;
        newtabAffected = true;
      }

      const homePageURL = HomePage.get();
      if (
        !["about:home", "about:blank"].includes(homePageURL) &&
        !homePageURL.startsWith("moz-extension://")
      ) {
        value.home_url_category = await this._classifySite(homePageURL);
        homeAffected = true;
      }
      const homeExtensionInfo = ExtensionSettingsStore.getSetting(
        "prefs",
        "homepage_override"
      );
      if (homeExtensionInfo && homeExtensionInfo.id) {
        value.home_extension_id = homeExtensionInfo.id;
        homeAffected = true;
      }

      let page;
      if (newtabAffected && homeAffected) {
        page = "both";
      } else if (newtabAffected) {
        page = "about:newtab";
      } else if (homeAffected) {
        page = "about:home";
      }

      if (page) {
        const event = Object.assign(this.createPing(), {
          action: "activity_stream_user_event",
          event: "PAGE_TAKEOVER_DATA",
          value,
          page,
          session_id: "n/a",
        });
        this.sendEvent(event);
      }
    }
  }

  onAction(action) {
    switch (action.type) {
      case at.INIT:
        this.init();
        this.sendPageTakeoverData();
        break;
      case at.NEW_TAB_INIT:
        this.handleNewTabInit(action);
        break;
      case at.NEW_TAB_UNLOAD:
        this.endSession(au.getPortIdOfSender(action));
        break;
      case at.SAVE_SESSION_PERF_DATA:
        this.saveSessionPerfData(au.getPortIdOfSender(action), action.data);
        break;
      case at.TELEMETRY_IMPRESSION_STATS:
        this.handleImpressionStats(action);
        break;
      case at.DISCOVERY_STREAM_IMPRESSION_STATS:
        this.handleDiscoveryStreamImpressionStats(
          au.getPortIdOfSender(action),
          action.data
        );
        break;
      case at.DISCOVERY_STREAM_LOADED_CONTENT:
        this.handleDiscoveryStreamLoadedContent(
          au.getPortIdOfSender(action),
          action.data
        );
        break;
      case at.DISCOVERY_STREAM_SPOCS_FILL:
        this.handleDiscoveryStreamSpocsFill(action.data);
        break;
      case at.TELEMETRY_UNDESIRED_EVENT:
        this.handleUndesiredEvent(action);
        break;
      case at.TELEMETRY_USER_EVENT:
        this.handleUserEvent(action);
        break;
      case at.AS_ROUTER_TELEMETRY_USER_EVENT:
        this.handleASRouterUserEvent(action);
        break;
      case at.TELEMETRY_PERFORMANCE_EVENT:
        this.sendEvent(this.createPerformanceEvent(action));
        break;
      case at.TRAILHEAD_ENROLL_EVENT:
        this.handleTrailheadEnrollEvent(action);
        break;
      case at.UNINIT:
        this.uninit();
        break;
    }
  }

  /**
   * Handle impression stats actions from Discovery Stream. The data will be
   * stored into the session.impressionSets object for the given port, so that
   * it is sent to the server when the session ends.
   *
   * @note session.impressionSets will be keyed on `source` of the `data`,
   * all the data will be appended to an array for the same source.
   *
   * @param {String} port  The session port with which this is associated
   * @param {Object} data  The impression data structured as {source: "SOURCE", tiles: [{id: 123}]}
   *
   */
  handleDiscoveryStreamImpressionStats(port, data) {
    let session = this.sessions.get(port);

    if (!session) {
      throw new Error("Session does not exist.");
    }

    const impressionSets = session.impressionSets || {};
    const impressions = impressionSets[data.source] || [];
    // The payload might contain other properties, we need `id`, `pos` and potentially `shim` here.
    data.tiles.forEach(tile =>
      impressions.push({
        id: tile.id,
        pos: tile.pos,
        ...(tile.shim ? { shim: tile.shim } : {}),
      })
    );
    impressionSets[data.source] = impressions;
    session.impressionSets = impressionSets;
  }

  /**
   * Handle loaded content actions from Discovery Stream. The data will be
   * stored into the session.loadedContentSets object for the given port, so that
   * it is sent to the server when the session ends.
   *
   * @note session.loadedContentSets will be keyed on `source` of the `data`,
   * all the data will be appended to an array for the same source.
   *
   * @param {String} port  The session port with which this is associated
   * @param {Object} data  The loaded content structured as {source: "SOURCE", tiles: [{id: 123}]}
   *
   */
  handleDiscoveryStreamLoadedContent(port, data) {
    let session = this.sessions.get(port);

    if (!session) {
      throw new Error("Session does not exist.");
    }

    const loadedContentSets = session.loadedContentSets || {};
    const loadedContents = loadedContentSets[data.source] || [];
    // The payload might contain other properties, we need `id` and `pos` here.
    data.tiles.forEach(tile =>
      loadedContents.push({ id: tile.id, pos: tile.pos })
    );
    loadedContentSets[data.source] = loadedContents;
    session.loadedContentSets = loadedContentSets;
  }

  /**
   * Handl SPOCS Fill actions from Discovery Stream.
   *
   * @param {Object} data
   *   The SPOCS Fill event structured as:
   *   {
   *     spoc_fills: [
   *       {
   *         id: 123,
   *         displayed: 0,
   *         reason: "frequency_cap",
   *         full_recalc: 1
   *        },
   *        {
   *          id: 124,
   *          displayed: 1,
   *          reason: "n/a",
   *          full_recalc: 1
   *        }
   *      ]
   *    }
   */
  handleDiscoveryStreamSpocsFill(data) {
    const payload = this.createSpocsFillPing(data);
    this.sendStructuredIngestionEvent(payload, "spoc-fills", "1");
  }

  /**
   * Take all enumerable members of the data object and merge them into
   * the session.perf object for the given port, so that it is sent to the
   * server when the session ends.  All members of the data object should
   * be valid values of the perf object, as defined in pings.js and the
   * data*.md documentation.
   *
   * @note Any existing keys with the same names already in the
   * session perf object will be overwritten by values passed in here.
   *
   * @param {String} port  The session with which this is associated
   * @param {Object} data  The perf data to be
   */
  saveSessionPerfData(port, data) {
    // XXX should use try/catch and send a bad state indicator if this
    // get blows up.
    let session = this.sessions.get(port);

    // XXX Partial workaround for #3118; avoids the worst incorrect associations
    // of times with browsers, by associating the load trigger with the
    // visibility event as the user is most likely associating the trigger to
    // the tab just shown. This helps avoid associating with a preloaded
    // browser as those don't get the event until shown. Better fix for more
    // cases forthcoming.
    //
    // XXX the about:home check (and the corresponding test) should go away
    // once the load_trigger stuff in addSession is refactored into
    // setLoadTriggerInfo.
    //
    if (data.visibility_event_rcvd_ts && session.page !== "about:home") {
      this.setLoadTriggerInfo(port);
    }

    let timestamp = data.topsites_first_painted_ts;

    if (
      timestamp &&
      session.page === "about:home" &&
      !HomePage.overridden &&
      Services.prefs.getIntPref("browser.startup.page") === 1
    ) {
      aboutNewTabService.maybeRecordTopsitesPainted(timestamp);
    }

    Object.assign(session.perf, data);
  }

  uninit() {
    try {
      Services.obs.removeObserver(
        this.browserOpenNewtabStart,
        "browser-open-newtab-start"
      );
      Services.obs.removeObserver(
        this._addWindowListeners,
        DOMWINDOW_OPENED_TOPIC
      );
    } catch (e) {
      // Operation can fail when uninit is called before
      // init has finished setting up the observer
    }

    // Only uninit if the getter has initialized it
    if (Object.prototype.hasOwnProperty.call(this, "pingCentre")) {
      this.pingCentre.uninit();
    }
    if (Object.prototype.hasOwnProperty.call(this, "utEvents")) {
      this.utEvents.uninit();
    }
    if (Object.prototype.hasOwnProperty.call(this, "pingCentreForASRouter")) {
      this.pingCentreForASRouter.uninit();
    }

    // TODO: Send any unfinished sessions
  }
};

const EXPORTED_SYMBOLS = [
  "TelemetryFeed",
  "USER_PREFS_ENCODING",
  "PREF_IMPRESSION_ID",
  "TELEMETRY_PREF",
  "EVENTS_TELEMETRY_PREF",
  "STRUCTURED_INGESTION_TELEMETRY_PREF",
  "STRUCTURED_INGESTION_ENDPOINT_PREF",
];
