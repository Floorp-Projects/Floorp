/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {utils: Cu} = Components;
const {actionTypes: at, actionUtils: au} = Cu.import("resource://activity-stream/common/Actions.jsm", {});

Cu.import("resource://gre/modules/ClientID.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "gUUIDGenerator",
  "@mozilla.org/uuid-generator;1",
  "nsIUUIDGenerator");
XPCOMUtils.defineLazyModuleGetter(this, "TelemetrySender",
  "resource://activity-stream/lib/TelemetrySender.jsm");

this.TelemetryFeed = class TelemetryFeed {
  constructor(options) {
    this.sessions = new Map();
    this.telemetryClientId = null;
    this.telemetrySender = null;
  }

  async init() {
    // TelemetrySender adds pref observers, so we initialize it after INIT
    this.telemetrySender = new TelemetrySender();

    const id = await ClientID.getClientID();
    this.telemetryClientId = id;
  }

  /**
   * addSession - Start tracking a new session
   *
   * @param  {string} id the portID of the open session
   */
  addSession(id) {
    this.sessions.set(id, {
      start_time: Components.utils.now(),
      session_id: String(gUUIDGenerator.generateUUID()),
      page: "about:newtab" // TODO: Handle about:home
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
  createPing(portID) {
    const appInfo = this.store.getState().App;
    const ping = {
      client_id: this.telemetryClientId,
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
      this.createPing(au.getPortIdOfSender(action)),
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
        action: "activity_stream_session"
      }
    );
  }

  sendEvent(event) {
    this.telemetrySender.sendPing(event);
  }

  onAction(action) {
    switch (action.type) {
      case at.INIT:
        this.init();
        break;
      case at.NEW_TAB_VISIBLE:
        this.addSession(au.getPortIdOfSender(action));
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
    this.telemetrySender.uninit();
    this.telemetrySender = null;
    // TODO: Send any unfinished sessions
  }
};

this.EXPORTED_SYMBOLS = ["TelemetryFeed"];
