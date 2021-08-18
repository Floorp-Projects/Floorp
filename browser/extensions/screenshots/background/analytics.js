/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals main, auth, browser, catcher, deviceInfo, log */

"use strict";

this.analytics = (function() {
  const exports = {};

  let myGaSegment = 1;
  let telemetryPrefKnown = false;
  let telemetryEnabled;
  // If there's this many entirely failed responses (e.g., server can't be contacted), then stop sending events
  // for the rest of the session:
  let serverFailedResponses = 3;

  const EVENT_BATCH_DURATION = 1000; // ms for setTimeout
  let pendingEvents = [];
  let pendingTimings = [];
  let eventsTimeoutHandle, timingsTimeoutHandle;
  const fetchOptions = {
    method: "POST",
    mode: "cors",
    headers: { "content-type": "application/json" },
    credentials: "include",
  };

  function shouldSendEvents() {
    return false;
  }

  function flushEvents() {
    if (pendingEvents.length === 0) {
      return;
    }

    const eventsUrl = `${main.getBackend()}/event`;
    const sendTime = Date.now();

    pendingEvents.forEach(event => {
      event.queueTime = sendTime - event.eventTime;
      log.info(
        `sendEvent ${event.event}/${event.action}/${event.label ||
          "none"} ${JSON.stringify(event.options)}`
      );
    });

    const body = JSON.stringify({ events: pendingEvents });
    const fetchRequest = fetch(
      eventsUrl,
      Object.assign({ body }, fetchOptions)
    );
    fetchWatcher(fetchRequest);
    pendingEvents = [];
  }

  function flushTimings() {
    if (pendingTimings.length === 0) {
      return;
    }

    const timingsUrl = `${main.getBackend()}/timing`;
    const body = JSON.stringify({ timings: pendingTimings });
    const fetchRequest = fetch(
      timingsUrl,
      Object.assign({ body }, fetchOptions)
    );
    fetchWatcher(fetchRequest);
    pendingTimings.forEach(t => {
      log.info(
        `sendTiming ${t.timingCategory}/${t.timingLabel}/${t.timingVar}: ${t.timingValue}`
      );
    });
    pendingTimings = [];
  }

  function sendTiming(timingLabel, timingVar, timingValue) {
    // sendTiming is only called in response to sendEvent, so no need to check
    // the telemetry pref again here.
    if (!shouldSendEvents()) {
      return;
    }
    const timingCategory = "addon";
    pendingTimings.push({
      timingCategory,
      timingLabel,
      timingVar,
      timingValue,
    });
    if (!timingsTimeoutHandle) {
      timingsTimeoutHandle = setTimeout(() => {
        timingsTimeoutHandle = null;
        flushTimings();
      }, EVENT_BATCH_DURATION);
    }
  }

  exports.sendEvent = function(action, label, options) {
    const eventCategory = "addon";
    if (!telemetryPrefKnown) {
      log.warn("sendEvent called before we were able to refresh");
      return Promise.resolve();
    }
    if (!telemetryEnabled) {
      log.info(
        `Cancelled sendEvent ${eventCategory}/${action}/${label ||
          "none"} ${JSON.stringify(options)}`
      );
      return Promise.resolve();
    }
    measureTiming(action, label);
    // Internal-only events are used for measuring time between events,
    // but aren't submitted to GA.
    if (action === "internal") {
      return Promise.resolve();
    }
    if (typeof label === "object" && !options) {
      options = label;
      label = undefined;
    }
    options = options || {};

    // Don't send events if in private browsing.
    if (options.incognito) {
      return Promise.resolve();
    }

    // Don't include in event data.
    delete options.incognito;

    const di = deviceInfo();
    options.applicationName = di.appName;
    options.applicationVersion = di.addonVersion;
    const abTests = auth.getAbTests();
    for (const [gaField, value] of Object.entries(abTests)) {
      options[gaField] = value;
    }
    if (!shouldSendEvents()) {
      // We don't want to save or send the events anymore
      return Promise.resolve();
    }
    pendingEvents.push({
      eventTime: Date.now(),
      event: eventCategory,
      action,
      label,
      options,
    });
    if (!eventsTimeoutHandle) {
      eventsTimeoutHandle = setTimeout(() => {
        eventsTimeoutHandle = null;
        flushEvents();
      }, EVENT_BATCH_DURATION);
    }
    // This function used to return a Promise that was not used at any of the
    // call sites; doing this simply maintains that interface.
    return Promise.resolve();
  };

  exports.incrementCount = function(scalar) {
    const allowedScalars = [
      "download",
      "upload",
      "copy",
      "visible",
      "full_page",
      "custom",
      "element",
    ];
    if (!allowedScalars.includes(scalar)) {
      const err = `incrementCount passed an unrecognized scalar ${scalar}`;
      log.warn(err);
      return Promise.resolve();
    }
    return browser.telemetry
      .scalarAdd(`screenshots.${scalar}`, 1)
      .catch(err => {
        log.warn(`incrementCount failed with error: ${err}`);
      });
  };

  exports.refreshTelemetryPref = function() {
    return browser.telemetry.canUpload().then(
      result => {
        telemetryPrefKnown = true;
        telemetryEnabled = result;
      },
      error => {
        // If there's an error reading the pref, we should assume that we shouldn't send data
        telemetryPrefKnown = true;
        telemetryEnabled = false;
        throw error;
      }
    );
  };

  exports.isTelemetryEnabled = function() {
    catcher.watchPromise(exports.refreshTelemetryPref());
    return telemetryEnabled;
  };

  const timingData = new Map();

  // Configuration for filtering the sendEvent stream on start/end events.
  // When start or end events occur, the time is recorded.
  // When end events occur, the elapsed time is calculated and submitted
  // via `sendEvent`, where action = "perf-response-time", label = name of rule,
  // and cd1 value is the elapsed time in milliseconds.
  // If a cancel event happens between the start and end events, the start time
  // is deleted.
  const rules = [
    {
      name: "page-action",
      start: { action: "start-shot", label: "toolbar-button" },
      end: { action: "internal", label: "unhide-preselection-frame" },
      cancel: [
        { action: "cancel-shot" },
        { action: "internal", label: "document-hidden" },
        { action: "internal", label: "unhide-onboarding-frame" },
      ],
    },
    {
      name: "context-menu",
      start: { action: "start-shot", label: "context-menu" },
      end: { action: "internal", label: "unhide-preselection-frame" },
      cancel: [
        { action: "cancel-shot" },
        { action: "internal", label: "document-hidden" },
        { action: "internal", label: "unhide-onboarding-frame" },
      ],
    },
    {
      name: "page-action-onboarding",
      start: { action: "start-shot", label: "toolbar-button" },
      end: { action: "internal", label: "unhide-onboarding-frame" },
      cancel: [
        { action: "cancel-shot" },
        { action: "internal", label: "document-hidden" },
        { action: "internal", label: "unhide-preselection-frame" },
      ],
    },
    {
      name: "context-menu-onboarding",
      start: { action: "start-shot", label: "context-menu" },
      end: { action: "internal", label: "unhide-onboarding-frame" },
      cancel: [
        { action: "cancel-shot" },
        { action: "internal", label: "document-hidden" },
        { action: "internal", label: "unhide-preselection-frame" },
      ],
    },
    {
      name: "capture-full-page",
      start: { action: "capture-full-page" },
      end: { action: "internal", label: "unhide-preview-frame" },
      cancel: [
        { action: "cancel-shot" },
        { action: "internal", label: "document-hidden" },
      ],
    },
    {
      name: "capture-visible",
      start: { action: "capture-visible" },
      end: { action: "internal", label: "unhide-preview-frame" },
      cancel: [
        { action: "cancel-shot" },
        { action: "internal", label: "document-hidden" },
      ],
    },
    {
      name: "make-selection",
      start: { action: "make-selection" },
      end: { action: "internal", label: "unhide-selection-frame" },
      cancel: [
        { action: "cancel-shot" },
        { action: "internal", label: "document-hidden" },
      ],
    },
    {
      name: "save-visible",
      start: { action: "save-visible" },
      end: { action: "internal", label: "open-shot-tab" },
      cancel: [{ action: "cancel-shot" }, { action: "upload-failed" }],
    },
    {
      name: "save-full-page",
      start: { action: "save-full-page" },
      end: { action: "internal", label: "open-shot-tab" },
      cancel: [{ action: "cancel-shot" }, { action: "upload-failed" }],
    },
    {
      name: "save-full-page-truncated",
      start: { action: "save-full-page-truncated" },
      end: { action: "internal", label: "open-shot-tab" },
      cancel: [{ action: "cancel-shot" }, { action: "upload-failed" }],
    },
    {
      name: "download-shot",
      start: { action: "download-shot" },
      end: { action: "internal", label: "deactivate" },
      cancel: [
        { action: "cancel-shot" },
        { action: "internal", label: "document-hidden" },
      ],
    },
    {
      name: "download-full-page",
      start: { action: "download-full-page" },
      end: { action: "internal", label: "deactivate" },
      cancel: [
        { action: "cancel-shot" },
        { action: "internal", label: "document-hidden" },
      ],
    },
    {
      name: "download-full-page-truncated",
      start: { action: "download-full-page-truncated" },
      end: { action: "internal", label: "deactivate" },
      cancel: [
        { action: "cancel-shot" },
        { action: "internal", label: "document-hidden" },
      ],
    },
    {
      name: "download-visible",
      start: { action: "download-visible" },
      end: { action: "internal", label: "deactivate" },
      cancel: [
        { action: "cancel-shot" },
        { action: "internal", label: "document-hidden" },
      ],
    },
  ];

  // Match a filter (action and optional label) against an action and label.
  function match(filter, action, label) {
    return filter.label
      ? filter.action === action && filter.label === label
      : filter.action === action;
  }

  function anyMatches(filters, action, label) {
    return filters.some(filter => match(filter, action, label));
  }

  function measureTiming(action, label) {
    rules.forEach(r => {
      if (anyMatches(r.cancel, action, label)) {
        delete timingData[r.name];
      } else if (match(r.start, action, label)) {
        timingData[r.name] = Math.round(performance.now());
      } else if (timingData[r.name] && match(r.end, action, label)) {
        const endTime = Math.round(performance.now());
        const elapsed = endTime - timingData[r.name];
        sendTiming("perf-response-time", r.name, elapsed);
        delete timingData[r.name];
      }
    });
  }

  function fetchWatcher(request) {
    request
      .then(response => {
        if (response.status === 410 || response.status === 404) {
          // Gone
          pendingEvents = [];
          pendingTimings = [];
        }
        if (!response.ok) {
          log.debug(
            `Error code in event response: ${response.status} ${response.statusText}`
          );
        }
      })
      .catch(error => {
        serverFailedResponses--;
        if (serverFailedResponses <= 0) {
          log.info(`Server is not responding, no more events will be sent`);
          pendingEvents = [];
          pendingTimings = [];
        }
        log.debug(`Error event in response: ${error}`);
      });
  }

  async function init() {
    const result = await browser.storage.local.get(["myGaSegment"]);
    if (!result.myGaSegment) {
      myGaSegment = Math.random();
      await browser.storage.local.set({ myGaSegment });
    } else {
      myGaSegment = result.myGaSegment;
    }
  }

  init();

  return exports;
})();
