/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* globals Services */

"use strict";

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

const {actionTypes: at, actionUtils: au} = ChromeUtils.import("resource://activity-stream/common/Actions.jsm", {});
const {Prefs} = ChromeUtils.import("resource://activity-stream/lib/ActivityStreamPrefs.jsm", {});

ChromeUtils.defineModuleGetter(this, "perfService",
  "resource://activity-stream/common/PerfService.jsm");
ChromeUtils.defineModuleGetter(this, "PingCentre",
  "resource:///modules/PingCentre.jsm");
ChromeUtils.defineModuleGetter(this, "UTEventReporting",
  "resource://activity-stream/lib/UTEventReporting.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "gUUIDGenerator",
  "@mozilla.org/uuid-generator;1",
  "nsIUUIDGenerator");

const ACTIVITY_STREAM_ID = "activity-stream";
const ACTIVITY_STREAM_ENDPOINT_PREF = "browser.newtabpage.activity-stream.telemetry.ping.endpoint";
const ACTIVITY_STREAM_ROUTER_ID = "activity-stream-router";

// This is a mapping table between the user preferences and its encoding code
const USER_PREFS_ENCODING = {
  "showSearch": 1 << 0,
  "feeds.topsites": 1 << 1,
  "feeds.section.topstories": 1 << 2,
  "feeds.section.highlights": 1 << 3,
  "feeds.snippets": 1 << 4,
  "showSponsored": 1 << 5
};

const PREF_IMPRESSION_ID = "impressionId";
const TELEMETRY_PREF = "telemetry";
const EVENTS_TELEMETRY_PREF = "telemetry.ut.events";

this.TelemetryFeed = class TelemetryFeed {
  constructor(options) {
    this.sessions = new Map();
    this._prefs = new Prefs();
    this._impressionId = this.getOrCreateImpressionId();
    this.telemetryEnabled = this._prefs.get(TELEMETRY_PREF);
    this.eventTelemetryEnabled = this._prefs.get(EVENTS_TELEMETRY_PREF);
    this._aboutHomeSeen = false;
    this._onTelemetryPrefChange = this._onTelemetryPrefChange.bind(this);
    this._prefs.observe(TELEMETRY_PREF, this._onTelemetryPrefChange);
    this._onEventsTelemetryPrefChange = this._onEventsTelemetryPrefChange.bind(this);
    this._prefs.observe(EVENTS_TELEMETRY_PREF, this._onEventsTelemetryPrefChange);
  }

  init() {
    Services.obs.addObserver(this.browserOpenNewtabStart, "browser-open-newtab-start");
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
        load_trigger_ts: perfService.getMostRecentAbsMarkStartByName("browser-open-newtab-start"),
        load_trigger_type: "menu_plus_or_keyboard"
      };
    } catch (e) {
      // if no mark was returned, we have nothing to save
      return;
    }
    this.saveSessionPerfData(port, data_to_save);
  }

  _onTelemetryPrefChange(prefVal) {
    this.telemetryEnabled = prefVal;
  }

  _onEventsTelemetryPrefChange(prefVal) {
    this.eventTelemetryEnabled = prefVal;
  }

  /**
   * Lazily initialize PingCentre for Activity Stream to send pings
   */
  get pingCentre() {
    Object.defineProperty(this, "pingCentre",
      {
        value: new PingCentre({
          topic: ACTIVITY_STREAM_ID,
          overrideEndpointPref: ACTIVITY_STREAM_ENDPOINT_PREF
        })
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
    Object.defineProperty(this, "pingCentreForASRouter",
      {value: new PingCentre({topic: ACTIVITY_STREAM_ROUTER_ID})});
    return this.pingCentreForASRouter;
  }

  /**
   * Lazily initialize UTEventReporting to send pings
   */
  get utEvents() {
    Object.defineProperty(this, "utEvents", {value: new UTEventReporting()});
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
        is_prerendered: false
      }
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

    if (session.perf.visibility_event_rcvd_ts) {
      session.session_duration = Math.round(perfService.absNow() - session.perf.visibility_event_rcvd_ts);
    }

    let sessionEndEvent = this.createSessionEndEvent(session);
    this.sendEvent(sessionEndEvent);
    this.sendUTEvent(sessionEndEvent, this.utEvents.sendSessionEndEvent);
    this.sessions.delete(portID);
  }

  /**
   * handlePagePrerendered - Set the session as prerendered
   *
   * @param  {string} portID the portID of the target session
   */
  handlePagePrerendered(portID) {
    const session = this.sessions.get(portID);

    if (!session) {
      // It's possible the tab was never visible – in which case, there was no user session.
      return;
    }

    session.perf.is_prerendered = true;
  }

  /**
   * handleNewTabInit - Handle NEW_TAB_INIT, which creates a new session and sets the a flag
   *                    for session.perf based on whether or not this new tab is preloaded
   *
   * @param  {obj} action the Action object
   */
  handleNewTabInit(action) {
    const session = this.addSession(au.getPortIdOfSender(action), action.data.url);
    session.perf.is_preloaded = action.data.browser.getAttribute("preloadedState") === "preloaded";
  }

  /**
   * createPing - Create a ping with common properties
   *
   * @param  {string} id The portID of the session, if a session is relevant (optional)
   * @return {obj}    A telemetry ping
   */
  createPing(portID) {
    const appInfo = this.store.getState().App;
    const ping = {
      addon_version: appInfo.version,
      locale: Services.locale.getAppLocaleAsLangTag(),
      user_prefs: this.userPreferences
    };

    // If the ping is part of a user session, add session-related info
    if (portID) {
      const session = this.sessions.get(portID) || this.addSession(portID);
      Object.assign(ping, {session_id: session.session_id});

      if (session.page) {
        Object.assign(ping, {page: session.page});
      }
    }
    return ping;
  }

  /**
   * createImpressionStats - Create a ping for an impression stats
   *
   * @param  {ob} action The object with data to be included in the ping.
   *                     For some user interactions.
   * @return {obj}    A telemetry ping
   */
  createImpressionStats(action) {
    return Object.assign(
      this.createPing(au.getPortIdOfSender(action)),
      action.data,
      {
        action: "activity_stream_impression_stats",
        impression_id: this._impressionId,
        client_id: "n/a",
        session_id: "n/a"
      }
    );
  }

  createUserEvent(action) {
    return Object.assign(
      this.createPing(au.getPortIdOfSender(action)),
      action.data,
      {action: "activity_stream_user_event"}
    );
  }

  createUndesiredEvent(action) {
    return Object.assign(
      this.createPing(au.getPortIdOfSender(action)),
      {value: 0}, // Default value
      action.data,
      {action: "activity_stream_undesired_event"}
    );
  }

  createPerformanceEvent(action) {
    return Object.assign(
      this.createPing(),
      action.data,
      {action: "activity_stream_performance_event"}
    );
  }

  createSessionEndEvent(session) {
    return Object.assign(
      this.createPing(),
      {
        session_id: session.session_id,
        page: session.page,
        session_duration: session.session_duration,
        action: "activity_stream_session",
        perf: session.perf
      }
    );
  }

  createASRouterEvent(action) {
    const appInfo = this.store.getState().App;
    const ping = {
      client_id: "n/a",
      addon_version: appInfo.version,
      locale: Services.locale.getAppLocaleAsLangTag(),
      impression_id: this._impressionId
    };
    return Object.assign(ping, action.data);
  }

  sendEvent(event_object) {
    if (this.telemetryEnabled) {
      this.pingCentre.sendPing(event_object,
      {filter: ACTIVITY_STREAM_ID});
    }
  }

  sendUTEvent(event_object, eventFunction) {
    if (this.telemetryEnabled && this.eventTelemetryEnabled) {
      eventFunction(event_object);
    }
  }

  sendASRouterEvent(event_object) {
    if (this.telemetryEnabled) {
      this.pingCentreForASRouter.sendPing(event_object,
      {filter: ACTIVITY_STREAM_ID});
    }
  }

  handleImpressionStats(action) {
    this.sendEvent(this.createImpressionStats(action));
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

  onAction(action) {
    switch (action.type) {
      case at.INIT:
        this.init();
        break;
      case at.NEW_TAB_INIT:
        this.handleNewTabInit(action);
        break;
      case at.NEW_TAB_UNLOAD:
        this.endSession(au.getPortIdOfSender(action));
        break;
      case at.PAGE_PRERENDERED:
        this.handlePagePrerendered(au.getPortIdOfSender(action));
        break;
      case at.SAVE_SESSION_PERF_DATA:
        this.saveSessionPerfData(au.getPortIdOfSender(action), action.data);
        break;
      case at.TELEMETRY_IMPRESSION_STATS:
        this.handleImpressionStats(action);
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
      case at.UNINIT:
        this.uninit();
        break;
    }
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

    Object.assign(session.perf, data);
  }

  uninit() {
    try {
      Services.obs.removeObserver(this.browserOpenNewtabStart,
        "browser-open-newtab-start");
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

    try {
      this._prefs.ignore(TELEMETRY_PREF, this._onTelemetryPrefChange);
      this._prefs.ignore(EVENTS_TELEMETRY_PREF, this._onEventsTelemetryPrefChange);
    } catch (e) {
      Cu.reportError(e);
    }
    // TODO: Send any unfinished sessions
  }
};

const EXPORTED_SYMBOLS = [
  "TelemetryFeed",
  "USER_PREFS_ENCODING",
  "PREF_IMPRESSION_ID",
  "TELEMETRY_PREF",
  "EVENTS_TELEMETRY_PREF"
];
