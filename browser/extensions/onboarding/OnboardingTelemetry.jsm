/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["OnboardingTelemetry"];

const {utils: Cu} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetters(this, {
  PingCentre: "resource:///modules/PingCentre.jsm",
  Services: "resource://gre/modules/Services.jsm",
});
XPCOMUtils.defineLazyServiceGetter(this, "gUUIDGenerator",
  "@mozilla.org/uuid-generator;1", "nsIUUIDGenerator");

/**
 * We send 2 kinds (firefox-onboarding-event, firefox-onboarding-session) of pings to ping centre
 * server (they call it `topic`). The `internal` state in `topic` field means this event is used internaly to
 * track states and will not send out any message.
 *
 * To save server space and make query easier, we track session begin and end but only send pings
 * when session end. Therefore the server will get single "onboarding/overlay/notification-session"
 * event which includes both `session_begin` and `session_end` timestamp.
 *
 * We send `session_begin` and `session_end` timestamps instead of `session_duration` diff because
 * of analytics engineer's request.
 */
const EVENT_WHITELIST = {
  // track when click the notification close button
  "notification-close-button-click": {topic: "firefox-onboarding-event", category: "notification-interactions"},
  // track when click the notification Call-To-Action button
  "notification-cta-click": {topic: "firefox-onboarding-event", category: "notification-interactions"},
  // track when notification is shown
  "notification-session-begin": {topic: "internal"},
  // track when the notification closed
  "notification-session-end": {topic: "firefox-onboarding-session", category: "notification-interactions"},
  // init onboarding session with session_key and page url
  "onboarding-register-session": {topic: "internal"},
  // track when the onboarding script inited
  "onboarding-session-begin": {topic: "internal"},
  // track when the onboarding script destoryed
  "onboarding-session-end": {topic: "firefox-onboarding-session", category: "overlay-interactions"},
  // track when click the overlay Call-To-Action button
  "overlay-cta-click": {topic: "firefox-onboarding-event", category: "overlay-interactions"},
  // track when click or auto select the overlay navigate item
  "overlay-nav-click": {topic: "firefox-onboarding-event", category: "overlay-interactions"},
  // track when the overlay is shown
  "overlay-session-begin": {topic: "internal"},
  // track when the overlay is closed
  "overlay-session-end": {topic: "firefox-onboarding-session", category: "overlay-interactions"},
  // track when click the overlay "skip tour" button
  "overlay-skip-tour": {topic: "firefox-onboarding-event", category: "overlay-interactions"},
};
const ONBOARDING_STATE_DEFAULT = "default";
const ONBOARDING_ID = "onboarding";

let OnboardingTelemetry = {
  sessionProbe: null,
  eventProbe: null,
  state: {
    sessions: {},
  },

  init(startupData) {
    this.sessionProbe = new PingCentre({topic: "firefox-onboarding-session"});
    this.eventProbe = new PingCentre({topic: "firefox-onboarding-event"});
    this.state.addon_version = startupData.version;
  },

  // register per tab session data
  registerNewTelemetrySession(session_key, page) {
    if (this.state.sessions[session_key]) {
      return;
    }
    // session_key and page url are must have
    if (!session_key || !page) {
      throw new Error("session_key and page url are required for onboarding-register-session");
    }
    let session_id = gUUIDGenerator.generateUUID().toString();
    this.state.sessions[session_key] = {
      session_id,
      page,
    };
  },

  process(data) {
    let { event, page, session_key } = data;

    let topic = EVENT_WHITELIST[event] && EVENT_WHITELIST[event].topic;
    if (!topic) {
      throw new Error(`ping-centre doesn't know ${event}, only knows ${Object.keys(EVENT_WHITELIST)}`);
    }

    if (event === "onboarding-register-session") {
      this.registerNewTelemetrySession(session_key, page);
    }

    if (!this.state.sessions[session_key]) {
      throw new Error(`should pass valid session_key`);
    }

    if (topic === "internal") {
      switch (event) {
        case "onboarding-session-begin":
          this.state.sessions[session_key].onboarding_session_begin = Date.now();
          break;
        case "overlay-session-begin":
          this.state.sessions[session_key].overlay_session_begin = Date.now();
          break;
        case "notification-session-begin":
          this.state.sessions[session_key].notification_session_begin = Date.now();
          break;
      }
    } else {
      this._send(topic, data);
    }
  },

  // send out pings by topic
  _send(topic, data) {
    let {
      addon_version,
    } = this.state;
    let {
      event,
      tour_id = "",
      session_key,
    } = data;
    let {
      notification_session_begin,
      onboarding_session_begin,
      overlay_session_begin,
      page,
      session_id,
    } = this.state.sessions[session_key];
    let category = EVENT_WHITELIST[event].category;
    let session_begin;
    switch (topic) {
      case "firefox-onboarding-session":
        switch (event) {
          case "onboarding-session-end":
            if (!onboarding_session_begin) {
              throw new Error(`should fire onboarding-session-begin event before ${event}`);
            }
            event = "onboarding-session";
            session_begin = onboarding_session_begin;
            delete this.state.sessions[session_key];
            break;
          case "overlay-session-end":
            if (!overlay_session_begin) {
              throw new Error(`should fire overlay-session-begin event before ${event}`);
            }
            event = "overlay-session";
            session_begin = overlay_session_begin;
            break;
          case "notification-session-end":
            if (!notification_session_begin) {
              throw new Error(`should fire notification-session-begin event before ${event}`);
            }
            event = "notification-session";
            session_begin = notification_session_begin;
            break;
        }

        // the field is used to identify how user open the overlay (through default logo or watermark),
        // the number of open from notification can be retrieved via `notification-cta-click` event
        const tour_source = Services.prefs.getStringPref("browser.onboarding.state",
          ONBOARDING_STATE_DEFAULT);
        const session_end = Date.now();
        this.sessionProbe && this.sessionProbe.sendPing({
          addon_version,
          category,
          event,
          page,
          session_begin,
          session_end,
          session_id,
          tour_id,
          tour_source,
        }, {filter: ONBOARDING_ID});
        break;
      case "firefox-onboarding-event":
        let impression = (event === "notification-close-button-click" ||
          event === "notification-cta-click") ?
          Services.prefs.getIntPref("browser.onboarding.notification.prompt-count", 0) : -1;
        this.eventProbe && this.eventProbe.sendPing({
          addon_version,
          category,
          event,
          impression,
          page,
          session_id,
          tour_id,
        }, {filter: ONBOARDING_ID});
        break;
    }
  }
};
