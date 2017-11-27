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

// Flag to control if we want to send new/old telemetry
// TODO: remove this flag and the legacy code in Bug 1419996
const NEW_TABLE = false;

// Validate the content has non-empty string
function hasString(str) {
  return typeof str == "string" && str.length > 0;
}

// Validate the content is an empty string
function isEmptyString(str) {
  return typeof str == "string" && str === "";
}

// Validate the content is an interger
function isInteger(i) {
  return Number.isInteger(i);
}

// Validate the content is a positive interger
function isPositiveInteger(i) {
  return Number.isInteger(i) && i > 0;
}

// Validate the number is -1
function isMinusOne(num) {
  return num === -1;
}

// Validate the category value is within the list
function isValidCategory(category) {
  return ["logo-interactions", "onboarding-interactions",
    "overlay-interactions", "notification-interactions"]
    .includes(category);
}

// Validate the page value is within the list
function isValidPage(page) {
  return ["about:newtab", "about:home"].includes(page);
}

// Validate the tour_type value is within the list
function isValidTourType(type) {
  return ["new", "update"].includes(type);
}

// Validate the bubble state value is within the list
function isValidBubbleState(str) {
  return ["bubble", "dot", "hide"].includes(str);
}

// Validate the logo state value is within the list
function isValidLogoState(str) {
  return ["logo", "watermark"].includes(str);
}

// Validate the notification state value is within the list
function isValidNotificationState(str) {
  return ["show", "hide", "finished"].includes(str);
}

// Validate the column must be defined per ping
function definePerPing(column) {
  return function() {
    throw new Error(`Must define the '${column}' validator per ping because it is not the same for all pings`);
  };
}

// Basic validators for session pings
// client_id, locale are added by PingCentre, IP is added by server
// so no need check these column here
const BASIC_SESSION_SCHEMA = {
  addon_version: hasString,
  category: isValidCategory,
  page: isValidPage,
  parent_session_id: hasString,
  root_session_id: hasString,
  session_begin: isInteger,
  session_end: isInteger,
  session_id: hasString,
  tour_type: isValidTourType,
  type: hasString,
};

// Basic validators for event pings
// client_id, locale are added by PingCentre, IP is added by server
// so no need check these column here
const BASIC_EVENT_SCHEMA = {
  addon_version: hasString,
  bubble_state: definePerPing("bubble_state"),
  category: isValidCategory,
  current_tour_id: definePerPing("current_tour_id"),
  logo_state: definePerPing("logo_state"),
  notification_impression: definePerPing("notification_impression"),
  notification_state: definePerPing("notification_state"),
  page: isValidPage,
  parent_session_id: hasString,
  root_session_id: hasString,
  target_tour_id: definePerPing("target_tour_id"),
  timestamp: isInteger,
  tour_type: isValidTourType,
  type: hasString,
  width: isPositiveInteger,
};

/**
 * We send 2 kinds (firefox-onboarding-event2, firefox-onboarding-session2) of pings to ping centre
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
  // track when a notification appears.
  "notification-appear": {
    topic: "firefox-onboarding-event2",
    category: "notification-interactions",
    validators: Object.assign({}, BASIC_EVENT_SCHEMA, {
      bubble_state: isValidBubbleState,
      current_tour_id: hasString,
      logo_state: isValidLogoState,
      notification_impression: isPositiveInteger,
      notification_state: isValidNotificationState,
      target_tour_id: isEmptyString,
    }),
  },
  // track when a user clicks close notification button
  "notification-close-button-click": {
    topic: "firefox-onboarding-event2",
    category: "notification-interactions",
    validators: Object.assign({}, BASIC_EVENT_SCHEMA, {
      bubble_state: isValidBubbleState,
      current_tour_id: hasString,
      logo_state: isValidLogoState,
      notification_impression: isPositiveInteger,
      notification_state: isValidNotificationState,
      target_tour_id: hasString,
    }),
  },
  // track when a user clicks notification's Call-To-Action button
  "notification-cta-click": {
    topic: "firefox-onboarding-event2",
    category: "notification-interactions",
    validators: Object.assign({}, BASIC_EVENT_SCHEMA, {
      bubble_state: isValidBubbleState,
      current_tour_id: hasString,
      logo_state: isValidLogoState,
      notification_impression: isPositiveInteger,
      notification_state: isValidNotificationState,
      target_tour_id: hasString,
    }),
  },
  // track the start and end time of the notification session
  "notification-session": {
    topic: "firefox-onboarding-session2",
    category: "notification-interactions",
    validators: BASIC_SESSION_SCHEMA,
  },
  // track the start of a notification
  "notification-session-begin": {topic: "internal"},
  // track the end of a notification
  "notification-session-end": {topic: "internal"},
  // track when a user clicks the Firefox logo
  "onboarding-logo-click": {
    topic: "firefox-onboarding-event2",
    category: "logo-interactions",
    validators: Object.assign({}, BASIC_EVENT_SCHEMA, {
      bubble_state: isValidBubbleState,
      current_tour_id: isEmptyString,
      logo_state: isValidLogoState,
      notification_impression: isMinusOne,
      notification_state: isValidNotificationState,
      target_tour_id: isEmptyString,
    }),
  },
  // track when the onboarding is not visisble due to small screen in the 1st load
  "onboarding-noshow-smallscreen": {
    topic: "firefox-onboarding-event2",
    category: "onboarding-interactions",
    validators: Object.assign({}, BASIC_EVENT_SCHEMA, {
      bubble_state: isEmptyString,
      current_tour_id: isEmptyString,
      logo_state: isEmptyString,
      notification_impression: isMinusOne,
      notification_state: isEmptyString,
      target_tour_id: isEmptyString,
    }),
  },
  // init onboarding session with session_key, page url, and tour_type
  "onboarding-register-session": {topic: "internal"},
  // track the start and end time of the onboarding session
  "onboarding-session": {
    topic: "firefox-onboarding-session2",
    category: "onboarding-interactions",
    validators: BASIC_SESSION_SCHEMA,
  },
  // track onboarding start time (when user loads about:home or about:newtab)
  "onboarding-session-begin": {topic: "internal"},
  // track onboarding end time (when user unloads about:home or about:newtab)
  "onboarding-session-end": {topic: "internal"},
  // track when a user clicks the close overlay button
  "overlay-close-button-click": {
    topic: "firefox-onboarding-event2",
    category: "overlay-interactions",
    validators: Object.assign({}, BASIC_EVENT_SCHEMA, {
      bubble_state: isEmptyString,
      current_tour_id: hasString,
      logo_state: isEmptyString,
      notification_impression: isMinusOne,
      notification_state: isEmptyString,
      target_tour_id: hasString,
    }),
  },
  // track when a user clicks outside the overlay area to end the tour
  "overlay-close-outside-click": {
    topic: "firefox-onboarding-event2",
    category: "overlay-interactions",
    validators: Object.assign({}, BASIC_EVENT_SCHEMA, {
      bubble_state: isEmptyString,
      current_tour_id: hasString,
      logo_state: isEmptyString,
      notification_impression: isMinusOne,
      notification_state: isEmptyString,
      target_tour_id: hasString,
    }),
  },
  // track when a user clicks overlay's Call-To-Action button
  "overlay-cta-click": {
    topic: "firefox-onboarding-event2",
    category: "overlay-interactions",
    validators: Object.assign({}, BASIC_EVENT_SCHEMA, {
      bubble_state: isEmptyString,
      current_tour_id: hasString,
      logo_state: isEmptyString,
      notification_impression: isMinusOne,
      notification_state: isEmptyString,
      target_tour_id: hasString,
    }),
  },
  // track when a tour is shown in the overlay
  "overlay-current-tour": {
    topic: "firefox-onboarding-event2",
    category: "overlay-interactions",
    validators: Object.assign({}, BASIC_EVENT_SCHEMA, {
      bubble_state: isEmptyString,
      current_tour_id: hasString,
      logo_state: isEmptyString,
      notification_impression: isMinusOne,
      notification_state: isEmptyString,
      target_tour_id: isEmptyString,
    }),
  },
  // track when an overlay is opened and disappeared because the window is resized too small
  "overlay-disapear-resize": {
    topic: "firefox-onboarding-event2",
    category: "overlay-interactions",
    validators: Object.assign({}, BASIC_EVENT_SCHEMA, {
      bubble_state: isEmptyString,
      current_tour_id: isEmptyString,
      logo_state: isEmptyString,
      notification_impression: isMinusOne,
      notification_state: isEmptyString,
      target_tour_id: isEmptyString,
    }),
  },
  // track when a user clicks a navigation button in the overlay
  "overlay-nav-click": {
    topic: "firefox-onboarding-event2",
    category: "overlay-interactions",
    validators: Object.assign({}, BASIC_EVENT_SCHEMA, {
      bubble_state: isEmptyString,
      current_tour_id: hasString,
      logo_state: isEmptyString,
      notification_impression: isMinusOne,
      notification_state: isEmptyString,
      target_tour_id: hasString,
    }),
  },
  // track the start and end time of the overlay session
  "overlay-session": {
    topic: "firefox-onboarding-session2",
    category: "overlay-interactions",
    validators:  BASIC_SESSION_SCHEMA,
  },
  // track the start of an overlay session
  "overlay-session-begin": {topic: "internal"},
  // track the end of an overlay session
  "overlay-session-end":  {topic: "internal"},
  // track when a user clicks 'Skip Tour' button in the overlay
  "overlay-skip-tour": {
    topic: "firefox-onboarding-event2",
    category: "overlay-interactions",
    validators: Object.assign({}, BASIC_EVENT_SCHEMA, {
      bubble_state: isEmptyString,
      current_tour_id: hasString,
      logo_state: isEmptyString,
      notification_impression: isMinusOne,
      notification_state: isEmptyString,
      target_tour_id: isEmptyString,
    }),
  },
};

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
const OLD_EVENT_WHITELIST = {
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
  registerNewTelemetrySession(data) {
    let { page, session_key, tour_type } = data;
    if (this.state.sessions[session_key]) {
      return;
    }
    // session_key and page url are must have
    if (!session_key || !page || !tour_type) {
      throw new Error("session_key, page url, and tour_type are required for onboarding-register-session");
    }
    let session_id = gUUIDGenerator.generateUUID().toString();
    this.state.sessions[session_key] = {
      page,
      session_id,
      tour_type,
    };
  },

  process(data) {
    if (NEW_TABLE) {
      throw new Error("Will implement in bug 1413830");
    } else {
      this.processOldPings(data);
    }
  },

  processOldPings(data) {
    let { event, session_key } = data;
    let topic = OLD_EVENT_WHITELIST[event] && OLD_EVENT_WHITELIST[event].topic;
    if (!topic) {
      throw new Error(`ping-centre doesn't know ${event}, only knows ${Object.keys(OLD_EVENT_WHITELIST)}`);
    }

    if (event === "onboarding-register-session") {
      this.registerNewTelemetrySession(data);
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
      this._sendOldPings(topic, data);
    }
  },

  // send out pings by topic
  _sendOldPings(topic, data) {
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
      tour_type,
    } = this.state.sessions[session_key];
    let category = OLD_EVENT_WHITELIST[event].category;
    // the field is used to identify how user open the overlay (through default logo or watermark),
    // the number of open from notification can be retrieved via `notification-cta-click` event
    let tour_source = Services.prefs.getStringPref("browser.onboarding.state", "default");
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

        let session_end = Date.now();
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
          tour_type,
        }, {filter: ONBOARDING_ID});
        break;
      case "firefox-onboarding-event":
        let impression = (event === "notification-close-button-click" ||
          event === "notification-cta-click") ?
          Services.prefs.getIntPref("browser.onboarding.notification.prompt-count", 0) : -1;
        let timestamp = Date.now();
        this.eventProbe && this.eventProbe.sendPing({
          addon_version,
          category,
          event,
          impression,
          page,
          session_id,
          timestamp,
          tour_id,
          tour_source,
          tour_type,
        }, {filter: ONBOARDING_ID});
        break;
    }
  },

  // validate data sanitation and make sure correct ping params are sent
  _validatePayload(payload) {
    let event = payload.type;
    let { validators } = EVENT_WHITELIST[event];
    if (!validators) {
      throw new Error(`Event ${event} without validators should not be sent.`);
    }
    let validatorKeys = Object.keys(validators);
    // Not send with undefined column
    if (Object.keys(payload).length > validatorKeys.length) {
      throw new Error(`Event ${event} want to send more columns than expect, should not be sent.`);
    }
    let results = {};
    let failed = false;
    // Per column validation
    for (let key of validatorKeys) {
      if (payload[key] !== undefined) {
        results[key] = validators[key](payload[key]);
        if (!results[key]) {
          failed = true;
        }
      } else {
        results[key] = false;
        failed = true;
      }
    }
    if (failed) {
      throw new Error(`Event ${event} contains incorrect data: ${JSON.stringify(results)}, should not be sent.`);
    }
  }
};
