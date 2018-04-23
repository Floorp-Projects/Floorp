/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["OnboardingTelemetry"];

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetters(this, {
  PingCentre: "resource:///modules/PingCentre.jsm",
});
XPCOMUtils.defineLazyServiceGetter(this, "gUUIDGenerator",
  "@mozilla.org/uuid-generator;1", "nsIUUIDGenerator");

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
    return ["about:newtab", "about:home", "about:welcome"].includes(page);
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
    parent: "notification-session",
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
    parent: "notification-session",
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
    parent: "notification-session",
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
    parent: "onboarding-session",
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
    parent: "onboarding-session",
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
    parent: "onboarding-session",
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
    parent: "onboarding-session",
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
    parent: "overlay-session",
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
    parent: "overlay-session",
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
    parent: "overlay-session",
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
    parent: "overlay-session",
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
    parent: "overlay-session",
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
    parent: "overlay-session",
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
    parent: "onboarding-session",
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
    parent: "overlay-session",
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

const ONBOARDING_ID = "onboarding";

let OnboardingTelemetry = {
  sessionProbe: null,
  eventProbe: null,
  state: {
    sessions: {},
  },

  init(startupData) {
    this.sessionProbe = new PingCentre({topic: "firefox-onboarding-session2"});
    this.eventProbe = new PingCentre({topic: "firefox-onboarding-event2"});
    this.state.addon_version = startupData.version;
  },

  // register per tab session data
  registerNewOnboardingSession(data) {
    let { page, session_key, tour_type } = data;
    if (this.state.sessions[session_key]) {
      return;
    }
    // session_key and page url are must have
    if (!session_key || !page || !tour_type) {
      throw new Error("session_key, page url, and tour_type are required for onboarding-register-session");
    }
    let onboarding_session_id = gUUIDGenerator.generateUUID().toString();
    this.state.sessions[session_key] = {
      onboarding_session_id,
      overlay_session_id: "",
      notification_session_id: "",
      page,
      tour_type,
    };
  },

  process(data) {
    let { type, session_key } = data;
    if (type === "onboarding-register-session") {
      this.registerNewOnboardingSession(data);
      return;
    }

    if (!this.state.sessions[session_key]) {
      throw new Error(`${type} should pass valid session_key`);
    }

    switch (type) {
      case "onboarding-session-begin":
        if (!this.state.sessions[session_key].onboarding_session_id) {
          throw new Error(`should fire onboarding-register-session event before ${type}`);
        }
        this.state.sessions[session_key].onboarding_session_begin = Date.now();
        return;
      case "onboarding-session-end":
        data = Object.assign({}, data, {
          type: "onboarding-session"
        });
        this.state.sessions[session_key].onboarding_session_end = Date.now();
        break;
      case "overlay-session-begin":
        this.state.sessions[session_key].overlay_session_id = gUUIDGenerator.generateUUID().toString();
        this.state.sessions[session_key].overlay_session_begin = Date.now();
        return;
      case "overlay-session-end":
        data = Object.assign({}, data, {
          type: "overlay-session"
        });
        this.state.sessions[session_key].overlay_session_end = Date.now();
        break;
      case "notification-session-begin":
        this.state.sessions[session_key].notification_session_id = gUUIDGenerator.generateUUID().toString();
        this.state.sessions[session_key].notification_session_begin = Date.now();
        return;
      case "notification-session-end":
        data = Object.assign({}, data, {
          type: "notification-session"
        });
        this.state.sessions[session_key].notification_session_end = Date.now();
        break;
    }
    let topic = EVENT_WHITELIST[data.type] && EVENT_WHITELIST[data.type].topic;
    if (!topic) {
      throw new Error(`ping-centre doesn't know ${type} after processPings, only knows ${Object.keys(EVENT_WHITELIST)}`);
    }
    this._sendPing(topic, data);
  },

  // send out pings by topic
  _sendPing(topic, data) {
    if (topic === "internal") {
      throw new Error(`internal ping ${data.type} should be processed within processPings`);
    }

    let {
      addon_version,
    } = this.state;
    let {
      bubble_state = "",
      current_tour_id = "",
      logo_state = "",
      notification_impression = -1,
      notification_state = "",
      session_key,
      target_tour_id = "",
      type,
      width,
    } = data;
    let {
      notification_session_begin,
      notification_session_end,
      notification_session_id,
      onboarding_session_begin,
      onboarding_session_end,
      onboarding_session_id,
      overlay_session_begin,
      overlay_session_end,
      overlay_session_id,
      page,
      tour_type,
    } = this.state.sessions[session_key];
    let {
      category,
      parent,
    } = EVENT_WHITELIST[type];
    let parent_session_id;
    let payload;
    let session_begin;
    let session_end;
    let session_id;
    let root_session_id = onboarding_session_id;

    // assign parent_session_id
    switch (parent) {
      case "onboarding-session":
        parent_session_id = onboarding_session_id;
        break;
      case "overlay-session":
        parent_session_id = overlay_session_id;
        break;
      case "notification-session":
        parent_session_id = notification_session_id;
        break;
    }
    if (!parent_session_id) {
      throw new Error(`Unable to find the ${parent} parent session for the event ${type}`);
    }

    switch (topic) {
      case "firefox-onboarding-session2":
        switch (type) {
          case "onboarding-session":
            session_id = onboarding_session_id;
            session_begin = onboarding_session_begin;
            session_end = onboarding_session_end;
            delete this.state.sessions[session_key];
            break;
          case "overlay-session":
            session_id = overlay_session_id;
            session_begin = overlay_session_begin;
            session_end = overlay_session_end;
            break;
          case "notification-session":
            session_id = notification_session_id;
            session_begin = notification_session_begin;
            session_end = notification_session_end;
            break;
        }
        if (!session_id || !session_begin || !session_end) {
          throw new Error(`should fire ${type}-begin and ${type}-end event before ${type}`);
        }

        payload = {
          addon_version,
          category,
          page,
          parent_session_id,
          root_session_id,
          session_begin,
          session_end,
          session_id,
          tour_type,
          type,
        };
        this._validatePayload(payload);
        this.sessionProbe && this.sessionProbe.sendPing(payload,
          {filter: ONBOARDING_ID});
        break;
      case "firefox-onboarding-event2":
        let timestamp = Date.now();
        payload = {
          addon_version,
          bubble_state,
          category,
          current_tour_id,
          logo_state,
          notification_impression,
          notification_state,
          page,
          parent_session_id,
          root_session_id,
          target_tour_id,
          timestamp,
          tour_type,
          type,
          width,
        };
        this._validatePayload(payload);
        this.eventProbe && this.eventProbe.sendPing(payload,
          {filter: ONBOARDING_ID});
        break;
    }
  },

  // validate data sanitation and make sure correct ping params are sent
  _validatePayload(payload) {
    let type = payload.type;
    let { validators } = EVENT_WHITELIST[type];
    if (!validators) {
      throw new Error(`Event ${type} without validators should not be sent.`);
    }
    let validatorKeys = Object.keys(validators);
    // Not send with undefined column
    if (Object.keys(payload).length > validatorKeys.length) {
      throw new Error(`Event ${type} want to send more columns than expect, should not be sent.`);
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
      throw new Error(`Event ${type} contains incorrect data: ${JSON.stringify(results)}, should not be sent.`);
    }
  }
};
