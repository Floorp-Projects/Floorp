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
   * @param  {number} absVisChangeTime absolute timestamp of
   *                                   document.visibilityState becoming visible
   */
  addSession(id, absVisChangeTime) {
    // XXX note that there is a race condition here; we're assuming that no
    // other tab will be interleaving calls to browserOpenNewtabStart and
    // addSession on this object.  For manually created windows, it's hard to
    // imagine us hitting this race condition.
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
    // introduce, rather than doing the correct by complicated thing.  It may
    // well be worth reexamining this hypothesis after we have more experience
    // with the data.
    let absBrowserOpenTabStart =
      perfService.getMostRecentAbsMarkStartByName("browser-open-newtab-start");

    this.sessions.set(id, {
      start_time: Components.utils.now(),
      session_id: String(gUUIDGenerator.generateUUID()),
      page: "about:newtab", // TODO: Handle about:home here and in perf below
      perf: {
        load_trigger_ts: absBrowserOpenTabStart,
        load_trigger_type: "menu_plus_or_keyboard",
        visibility_event_rcvd_ts: absVisChangeTime
      }
    });

    let duration = absVisChangeTime - absBrowserOpenTabStart;
    this.store.dispatch({
      type: at.TELEMETRY_PERFORMANCE_EVENT,
      data: {visability_duration: duration}
    });
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

    session.session_duration = Math.round(Components.utils.now() - session.start_time);
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
      const session = this.sessions.get(portID);
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
      await this.createPing(au.getPortIdOfSender(action)),
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
      case at.NEW_TAB_VISIBLE:
        this.addSession(au.getPortIdOfSender(action),
          action.data.absVisibilityChangeTime);
        break;
      case at.NEW_TAB_UNLOAD:
        this.endSession(au.getPortIdOfSender(action));
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
