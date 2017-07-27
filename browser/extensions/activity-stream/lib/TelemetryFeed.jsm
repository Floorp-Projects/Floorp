/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* globals Services */

"use strict";

const {interfaces: Ci, utils: Cu} = Components;
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const {actionTypes: at, actionUtils: au} = Cu.import("resource://activity-stream/common/Actions.jsm", {});

XPCOMUtils.defineLazyModuleGetter(this, "ClientID",
  "resource://gre/modules/ClientID.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "perfService",
  "resource://activity-stream/common/PerfService.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "TelemetrySender",
  "resource://activity-stream/lib/TelemetrySender.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "gUUIDGenerator",
  "@mozilla.org/uuid-generator;1",
  "nsIUUIDGenerator");

this.TelemetryFeed = class TelemetryFeed {
  constructor(options) {
    this.sessions = new Map();
  }

  init() {
    Services.obs.addObserver(this.browserOpenNewtabStart, "browser-open-newtab-start");
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

  /**
   * Lazily get the Telemetry id promise
   */
  get telemetryClientId() {
    Object.defineProperty(this, "telemetryClientId", {value: ClientID.getClientID()});
    return this.telemetryClientId;
  }

  /**
   * Lazily initialize TelemetrySender to send pings
   */
  get telemetrySender() {
    Object.defineProperty(this, "telemetrySender", {value: new TelemetrySender()});
    return this.telemetrySender;
  }

  /**
   * addSession - Start tracking a new session
   *
   * @param  {string} id the portID of the open session
   *
   * @return {obj}    Session object
   */
  addSession(id) {
    const session = {
      session_id: String(gUUIDGenerator.generateUUID()),
      page: "about:newtab", // TODO: Handle about:home here
      // "unexpected" will be overwritten when appropriate
      perf: {load_trigger_type: "unexpected"}
    };

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

    this.sendEvent(this.createSessionEndEvent(session));
    this.sessions.delete(portID);
  }

  /**
   * createPing - Create a ping with common properties
   *
   * @param  {string} id The portID of the session, if a session is relevant (optional)
   * @return {obj}    A telemetry ping
   */
  async createPing(portID) {
    const appInfo = this.store.getState().App;
    const ping = {
      client_id: await this.telemetryClientId,
      addon_version: appInfo.version,
      locale: appInfo.locale
    };

    // If the ping is part of a user session, add session-related info
    if (portID) {
      const session = this.sessions.get(portID) || this.addSession(portID);
      Object.assign(ping, {
        session_id: session.session_id,
        page: session.page
      });
    }
    return ping;
  }

  async createUserEvent(action) {
    return Object.assign(
      await this.createPing(au.getPortIdOfSender(action)),
      action.data,
      {action: "activity_stream_user_event"}
    );
  }

  async createUndesiredEvent(action) {
    return Object.assign(
      await this.createPing(au.getPortIdOfSender(action)),
      {value: 0}, // Default value
      action.data,
      {action: "activity_stream_undesired_event"}
    );
  }

  async createPerformanceEvent(action) {
    return Object.assign(
      await this.createPing(),
      action.data,
      {action: "activity_stream_performance_event"}
    );
  }

  async createSessionEndEvent(session) {
    return Object.assign(
      await this.createPing(),
      {
        session_id: session.session_id,
        page: session.page,
        session_duration: session.session_duration,
        action: "activity_stream_session",
        perf: session.perf
      }
    );
  }

  async sendEvent(eventPromise) {
    this.telemetrySender.sendPing(await eventPromise);
  }

  onAction(action) {
    switch (action.type) {
      case at.INIT:
        this.init();
        break;
      case at.NEW_TAB_INIT:
        this.addSession(au.getPortIdOfSender(action));
        this.setLoadTriggerInfo(au.getPortIdOfSender(action));
        break;
      case at.NEW_TAB_UNLOAD:
        this.endSession(au.getPortIdOfSender(action));
        break;
      case at.SAVE_SESSION_PERF_DATA:
        this.saveSessionPerfData(au.getPortIdOfSender(action), action.data);
        break;
      case at.TELEMETRY_UNDESIRED_EVENT:
        this.sendEvent(this.createUndesiredEvent(action));
        break;
      case at.TELEMETRY_USER_EVENT:
        this.sendEvent(this.createUserEvent(action));
        break;
      case at.TELEMETRY_PERFORMANCE_EVENT:
        this.sendEvent(this.createPerformanceEvent(action));
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

    Object.assign(session.perf, data);
  }

  uninit() {
    Services.obs.removeObserver(this.browserOpenNewtabStart,
      "browser-open-newtab-start");

    // Only uninit if the getter has initialized it
    if (Object.prototype.hasOwnProperty.call(this, "telemetrySender")) {
      this.telemetrySender.uninit();
    }
    // TODO: Send any unfinished sessions
  }
};

this.EXPORTED_SYMBOLS = ["TelemetryFeed"];
