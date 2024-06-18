/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// We use importESModule here instead of static import so that
// the Karma test environment won't choke on these module. This
// is because the Karma test environment already stubs out
// XPCOMUtils, and overrides importESModule to be a no-op (which
// can't be done for a static import statement). MESSAGE_TYPES_HASH / msg
// isn't something that the tests for this module seem to rely on in the
// Karma environment, but if that ever becomes the case, we should import
// those into unit-entry like we do for the ASRouter tests.

// eslint-disable-next-line mozilla/use-static-import
const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

// eslint-disable-next-line mozilla/use-static-import
const { MESSAGE_TYPE_HASH: msg } = ChromeUtils.importESModule(
  "resource:///modules/asrouter/ActorConstants.mjs"
);

import {
  actionTypes as at,
  actionUtils as au,
} from "resource://activity-stream/common/Actions.mjs";
import { Prefs } from "resource://activity-stream/lib/ActivityStreamPrefs.sys.mjs";
import { classifySite } from "resource://activity-stream/lib/SiteClassifier.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  AboutNewTab: "resource:///modules/AboutNewTab.sys.mjs",
  AboutWelcomeTelemetry:
    "resource:///modules/aboutwelcome/AboutWelcomeTelemetry.sys.mjs",
  ClientID: "resource://gre/modules/ClientID.sys.mjs",
  ExperimentAPI: "resource://nimbus/ExperimentAPI.sys.mjs",
  ExtensionSettingsStore:
    "resource://gre/modules/ExtensionSettingsStore.sys.mjs",
  HomePage: "resource:///modules/HomePage.sys.mjs",
  NimbusFeatures: "resource://nimbus/ExperimentAPI.sys.mjs",
  TelemetryEnvironment: "resource://gre/modules/TelemetryEnvironment.sys.mjs",
  TelemetrySession: "resource://gre/modules/TelemetrySession.sys.mjs",
  UTEventReporting: "resource://activity-stream/lib/UTEventReporting.sys.mjs",
  UpdateUtils: "resource://gre/modules/UpdateUtils.sys.mjs",
  pktApi: "chrome://pocket/content/pktApi.sys.mjs",
});
ChromeUtils.defineLazyGetter(
  lazy,
  "Telemetry",
  () => new lazy.AboutWelcomeTelemetry()
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "handoffToAwesomebarPrefValue",
  "browser.newtabpage.activity-stream.improvesearch.handoffToAwesomebar",
  false,
  (preference, previousValue, new_value) =>
    Glean.newtabHandoffPreference.enabled.set(new_value)
);

// This is a mapping table between the user preferences and its encoding code
export const USER_PREFS_ENCODING = {
  showSearch: 1 << 0,
  "feeds.topsites": 1 << 1,
  "feeds.section.topstories": 1 << 2,
  "feeds.section.highlights": 1 << 3,
  showSponsored: 1 << 5,
  "asrouter.userprefs.cfr.addons": 1 << 6,
  "asrouter.userprefs.cfr.features": 1 << 7,
  showSponsoredTopSites: 1 << 8,
};

export const PREF_IMPRESSION_ID = "impressionId";
export const TELEMETRY_PREF = "telemetry";
export const EVENTS_TELEMETRY_PREF = "telemetry.ut.events";

// Used as the missing value for timestamps in the session ping
const TIMESTAMP_MISSING_VALUE = -1;

// Page filter for onboarding telemetry, any value other than these will
// be set as "other"
const ONBOARDING_ALLOWED_PAGE_VALUES = [
  "about:welcome",
  "about:home",
  "about:newtab",
];

ChromeUtils.defineLazyGetter(
  lazy,
  "browserSessionId",
  () => lazy.TelemetrySession.getMetadata("").sessionId
);

// The scalar category for TopSites of Contextual Services
const SCALAR_CATEGORY_TOPSITES = "contextual.services.topsites";
// `contextId` is a unique identifier used by Contextual Services
const CONTEXT_ID_PREF = "browser.contextual-services.contextId";
ChromeUtils.defineLazyGetter(lazy, "contextId", () => {
  let _contextId = Services.prefs.getStringPref(CONTEXT_ID_PREF, null);
  if (!_contextId) {
    _contextId = String(Services.uuid.generateUUID());
    Services.prefs.setStringPref(CONTEXT_ID_PREF, _contextId);
  }
  return _contextId;
});

const ACTIVITY_STREAM_PREF_BRANCH = "browser.newtabpage.activity-stream.";
const NEWTAB_PING_PREFS = {
  showSearch: Glean.newtabSearch.enabled,
  "feeds.topsites": Glean.topsites.enabled,
  showSponsoredTopSites: Glean.topsites.sponsoredEnabled,
  "feeds.section.topstories": Glean.pocket.enabled,
  showSponsored: Glean.pocket.sponsoredStoriesEnabled,
  topSitesRows: Glean.topsites.rows,
  showWeather: Glean.newtab.weatherEnabled,
};
const TOP_SITES_BLOCKED_SPONSORS_PREF = "browser.topsites.blockedSponsors";

export class TelemetryFeed {
  constructor() {
    this.sessions = new Map();
    this._prefs = new Prefs();
    this._impressionId = this.getOrCreateImpressionId();
    this._aboutHomeSeen = false;
    this._classifySite = classifySite;
    this._browserOpenNewtabStart = null;
  }

  get telemetryEnabled() {
    return this._prefs.get(TELEMETRY_PREF);
  }

  get eventTelemetryEnabled() {
    return this._prefs.get(EVENTS_TELEMETRY_PREF);
  }

  get telemetryClientId() {
    Object.defineProperty(this, "telemetryClientId", {
      value: lazy.ClientID.getClientID(),
    });
    return this.telemetryClientId;
  }

  get processStartTs() {
    let startupInfo = Services.startup.getStartupInfo();
    let processStartTs = startupInfo.process.getTime();

    Object.defineProperty(this, "processStartTs", {
      value: processStartTs,
    });
    return this.processStartTs;
  }

  init() {
    this._beginObservingNewtabPingPrefs();
    Services.obs.addObserver(
      this.browserOpenNewtabStart,
      "browser-open-newtab-start"
    );
    // Set two scalars for the "deletion-request" ping (See bug 1602064 and 1729474)
    Services.telemetry.scalarSet(
      "deletion.request.impression_id",
      this._impressionId
    );
    Services.telemetry.scalarSet("deletion.request.context_id", lazy.contextId);
    Glean.newtab.locale.set(Services.locale.appLocaleAsBCP47);
    Glean.newtabHandoffPreference.enabled.set(
      lazy.handoffToAwesomebarPrefValue
    );
  }

  getOrCreateImpressionId() {
    let impressionId = this._prefs.get(PREF_IMPRESSION_ID);
    if (!impressionId) {
      impressionId = String(Services.uuid.generateUUID());
      this._prefs.set(PREF_IMPRESSION_ID, impressionId);
    }
    return impressionId;
  }

  browserOpenNewtabStart() {
    let now = Cu.now();
    this._browserOpenNewtabStart = Math.round(this.processStartTs + now);

    ChromeUtils.addProfilerMarker(
      "UserTiming",
      now,
      "browser-open-newtab-start"
    );
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
      if (!this._browserOpenNewtabStart) {
        throw new Error("No browser-open-newtab-start recorded.");
      }
      data_to_save = {
        load_trigger_ts: this._browserOpenNewtabStart,
        load_trigger_type: "menu_plus_or_keyboard",
      };
    } catch (e) {
      // if no mark was returned, we have nothing to save
      return;
    }
    this.saveSessionPerfData(port, data_to_save);
  }

  /**
   * Lazily initialize UTEventReporting to send pings
   */
  get utEvents() {
    Object.defineProperty(this, "utEvents", {
      value: new lazy.UTEventReporting(),
    });
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
   *  Check if it is in the CFR experiment cohort by querying against the
   *  experiment manager of Messaging System
   *
   *  @return {bool}
   */
  get isInCFRCohort() {
    const experimentData = lazy.ExperimentAPI.getExperimentMetaData({
      featureId: "cfr",
    });
    if (experimentData && experimentData.slug) {
      return true;
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
      // clicking of the icon. We express this by using the process start
      // absolute timestamp.
      load_trigger_ts = this.processStartTs;
    }

    const session = {
      session_id: String(Services.uuid.generateUUID()),
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

    Glean.newtab.closed.record({ newtab_visit_id: session.session_id });
    if (
      this.telemetryEnabled &&
      (lazy.NimbusFeatures.glean.getVariable("newtabPingEnabled") ?? true)
    ) {
      GleanPings.newtab.submit("newtab_session_end");
    }

    if (session.perf.visibility_event_rcvd_ts) {
      let absNow = this.processStartTs + Cu.now();
      session.session_duration = Math.round(
        absNow - session.perf.visibility_event_rcvd_ts
      );

      // Rounding all timestamps in perf to ease the data processing on the backend.
      // NB: use `TIMESTAMP_MISSING_VALUE` if the value is missing.
      session.perf.visibility_event_rcvd_ts = Math.round(
        session.perf.visibility_event_rcvd_ts
      );
      session.perf.load_trigger_ts = Math.round(
        session.perf.load_trigger_ts || TIMESTAMP_MISSING_VALUE
      );
      session.perf.topsites_first_painted_ts = Math.round(
        session.perf.topsites_first_painted_ts || TIMESTAMP_MISSING_VALUE
      );
    } else {
      // This session was never shown (i.e. the hidden preloaded newtab), there was no user session either.
      this.sessions.delete(portID);
      return;
    }

    let sessionEndEvent = this.createSessionEndEvent(session);
    this.sendUTEvent(sessionEndEvent, this.utEvents.sendSessionEndEvent);
    this.sessions.delete(portID);
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
      locale: Services.locale.appLocaleAsBCP47,
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

  createUserEvent(action) {
    return Object.assign(
      this.createPing(au.getPortIdOfSender(action)),
      action.data,
      { action: "activity_stream_user_event" }
    );
  }

  createSessionEndEvent(session) {
    return Object.assign(this.createPing(), {
      session_id: session.session_id,
      page: session.page,
      session_duration: session.session_duration,
      action: "activity_stream_session",
      perf: session.perf,
      profile_creation_date:
        lazy.TelemetryEnvironment.currentEnvironment.profile.resetDate ||
        lazy.TelemetryEnvironment.currentEnvironment.profile.creationDate,
    });
  }

  /**
   * Create a ping for AS router event. The client_id is set to "n/a" by default,
   * different component can override this by its own telemetry collection policy.
   */
  async createASRouterEvent(action) {
    let event = {
      ...action.data,
      addon_version: Services.appinfo.appBuildID,
      locale: Services.locale.appLocaleAsBCP47,
    };
    const session = this.sessions.get(au.getPortIdOfSender(action));
    if (event.event_context && typeof event.event_context === "object") {
      event.event_context = JSON.stringify(event.event_context);
    }
    switch (event.action) {
      case "cfr_user_event":
        event = await this.applyCFRPolicy(event);
        break;
      case "badge_user_event":
        event = await this.applyToolbarBadgePolicy(event);
        break;
      case "infobar_user_event":
        event = await this.applyInfoBarPolicy(event);
        break;
      case "spotlight_user_event":
        event = await this.applySpotlightPolicy(event);
        break;
      case "toast_notification_user_event":
        event = await this.applyToastNotificationPolicy(event);
        break;
      case "moments_user_event":
        event = await this.applyMomentsPolicy(event);
        break;
      case "onboarding_user_event":
        event = await this.applyOnboardingPolicy(event, session);
        break;
      case "asrouter_undesired_event":
        event = this.applyUndesiredEventPolicy(event);
        break;
      default:
        event = { ping: event };
        break;
    }
    return event;
  }

  /**
   * Per Bug 1484035, CFR metrics comply with following policies:
   * 1). In release, it collects impression_id and bucket_id
   * 2). In prerelease, it collects client_id and message_id
   * 3). In shield experiments conducted in release, it collects client_id and message_id
   * 4). In Private Browsing windows, unless in experiment, collects impression_id and bucket_id
   */
  async applyCFRPolicy(ping) {
    if (
      (lazy.UpdateUtils.getUpdateChannel(true) === "release" ||
        ping.is_private) &&
      !this.isInCFRCohort
    ) {
      ping.message_id = "n/a";
      ping.impression_id = this._impressionId;
    } else {
      ping.client_id = await this.telemetryClientId;
    }
    delete ping.action;
    delete ping.is_private;
    return { ping, pingType: "cfr" };
  }

  /**
   * Per Bug 1482134, all the metrics for What's New panel use client_id in
   * all the release channels
   */
  async applyToolbarBadgePolicy(ping) {
    ping.client_id = await this.telemetryClientId;
    ping.browser_session_id = lazy.browserSessionId;
    // Attach page info to `event_context` if there is a session associated with this ping
    delete ping.action;
    return { ping, pingType: "toolbar-badge" };
  }

  async applyInfoBarPolicy(ping) {
    ping.client_id = await this.telemetryClientId;
    ping.browser_session_id = lazy.browserSessionId;
    delete ping.action;
    return { ping, pingType: "infobar" };
  }

  async applySpotlightPolicy(ping) {
    ping.client_id = await this.telemetryClientId;
    ping.browser_session_id = lazy.browserSessionId;
    delete ping.action;
    return { ping, pingType: "spotlight" };
  }

  async applyToastNotificationPolicy(ping) {
    ping.client_id = await this.telemetryClientId;
    ping.browser_session_id = lazy.browserSessionId;
    delete ping.action;
    return { ping, pingType: "toast_notification" };
  }

  /**
   * Per Bug 1484035, Moments metrics comply with following policies:
   * 1). In release, it collects impression_id, and treats bucket_id as message_id
   * 2). In prerelease, it collects client_id and message_id
   * 3). In shield experiments conducted in release, it collects client_id and message_id
   */
  async applyMomentsPolicy(ping) {
    if (
      lazy.UpdateUtils.getUpdateChannel(true) === "release" &&
      !this.isInCFRCohort
    ) {
      ping.message_id = "n/a";
      ping.impression_id = this._impressionId;
    } else {
      ping.client_id = await this.telemetryClientId;
    }
    delete ping.action;
    return { ping, pingType: "moments" };
  }

  /**
   * Per Bug 1482134, all the metrics for Onboarding in AS router use client_id in
   * all the release channels
   */
  async applyOnboardingPolicy(ping, session) {
    ping.client_id = await this.telemetryClientId;
    ping.browser_session_id = lazy.browserSessionId;
    // Attach page info to `event_context` if there is a session associated with this ping
    if (ping.action === "onboarding_user_event" && session && session.page) {
      let event_context;

      try {
        event_context = ping.event_context
          ? JSON.parse(ping.event_context)
          : {};
      } catch (e) {
        // If `ping.event_context` is not a JSON serialized string, then we create a `value`
        // key for it
        event_context = { value: ping.event_context };
      }

      if (ONBOARDING_ALLOWED_PAGE_VALUES.includes(session.page)) {
        event_context.page = session.page;
      } else {
        console.error(`Invalid 'page' for Onboarding event: ${session.page}`);
      }
      ping.event_context = JSON.stringify(event_context);
    }
    delete ping.action;
    return { ping, pingType: "onboarding" };
  }

  applyUndesiredEventPolicy(ping) {
    ping.impression_id = this._impressionId;
    delete ping.action;
    return { ping, pingType: "undesired-events" };
  }

  sendUTEvent(event_object, eventFunction) {
    if (this.telemetryEnabled && this.eventTelemetryEnabled) {
      eventFunction(event_object);
    }
  }

  handleTopSitesSponsoredImpressionStats(action) {
    const { data } = action;
    const {
      type,
      position,
      source,
      advertiser: advertiser_name,
      tile_id,
    } = data;
    // Legacy telemetry expects 1-based tile positions.
    const legacyTelemetryPosition = position + 1;

    let pingType;

    const session = this.sessions.get(au.getPortIdOfSender(action));
    if (type === "impression") {
      pingType = "topsites-impression";
      Services.telemetry.keyedScalarAdd(
        `${SCALAR_CATEGORY_TOPSITES}.impression`,
        `${source}_${legacyTelemetryPosition}`,
        1
      );
      if (session) {
        Glean.topsites.impression.record({
          advertiser_name,
          tile_id,
          newtab_visit_id: session.session_id,
          is_sponsored: true,
          position,
        });
      }
    } else if (type === "click") {
      pingType = "topsites-click";
      Services.telemetry.keyedScalarAdd(
        `${SCALAR_CATEGORY_TOPSITES}.click`,
        `${source}_${legacyTelemetryPosition}`,
        1
      );
      if (session) {
        Glean.topsites.click.record({
          advertiser_name,
          tile_id,
          newtab_visit_id: session.session_id,
          is_sponsored: true,
          position,
        });
      }
    } else {
      console.error("Unknown ping type for sponsored TopSites impression");
      return;
    }

    Glean.topSites.pingType.set(pingType);
    Glean.topSites.position.set(legacyTelemetryPosition);
    Glean.topSites.source.set(source);
    Glean.topSites.tileId.set(tile_id);
    if (data.reporting_url) {
      Glean.topSites.reportingUrl.set(data.reporting_url);
    }
    Glean.topSites.advertiser.set(advertiser_name);
    Glean.topSites.contextId.set(lazy.contextId);
    GleanPings.topSites.submit();
  }

  handleTopSitesOrganicImpressionStats(action) {
    const session = this.sessions.get(au.getPortIdOfSender(action));
    if (!session) {
      return;
    }

    switch (action.data?.type) {
      case "impression":
        Glean.topsites.impression.record({
          newtab_visit_id: session.session_id,
          is_sponsored: false,
          position: action.data.position,
        });
        break;

      case "click":
        Glean.topsites.click.record({
          newtab_visit_id: session.session_id,
          is_sponsored: false,
          position: action.data.position,
        });
        break;

      default:
        break;
    }
  }

  handleUserEvent(action) {
    let userEvent = this.createUserEvent(action);
    this.sendUTEvent(userEvent, this.utEvents.sendUserEvent);
  }

  handleDiscoveryStreamUserEvent(action) {
    const pocket_logged_in_status = lazy.pktApi.isUserLoggedIn();
    Glean.pocket.isSignedIn.set(pocket_logged_in_status);
    this.handleUserEvent({
      ...action,
      data: {
        ...(action.data || {}),
        value: {
          ...(action.data?.value || {}),
          pocket_logged_in_status,
        },
      },
    });
    const session = this.sessions.get(au.getPortIdOfSender(action));
    switch (action.data?.event) {
      case "CLICK": {
        const {
          card_type,
          topic,
          recommendation_id,
          tile_id,
          shim,
          fetchTimestamp,
          firstVisibleTimestamp,
          feature,
        } = action.data.value ?? {};
        if (
          action.data.source === "POPULAR_TOPICS" ||
          card_type === "topics_widget"
        ) {
          Glean.pocket.topicClick.record({
            newtab_visit_id: session.session_id,
            topic,
          });
        } else if (action.data.source === "FEATURE_HIGHLIGHT") {
          Glean.newtab.tooltipClick.record({
            newtab_visit_id: session.session_id,
            feature,
          });
        } else if (["spoc", "organic"].includes(card_type)) {
          Glean.pocket.click.record({
            newtab_visit_id: session.session_id,
            is_sponsored: card_type === "spoc",
            position: action.data.action_position,
            recommendation_id,
            tile_id,
          });
          if (shim) {
            Glean.pocket.shim.set(shim);
            if (fetchTimestamp) {
              Glean.pocket.fetchTimestamp.set(fetchTimestamp * 1000);
            }
            if (firstVisibleTimestamp) {
              Glean.pocket.newtabCreationTimestamp.set(
                firstVisibleTimestamp * 1000
              );
            }
            GleanPings.spoc.submit("click");
          }
        }
        break;
      }
      case "SAVE_TO_POCKET":
        Glean.pocket.save.record({
          newtab_visit_id: session.session_id,
          is_sponsored: action.data.value?.card_type === "spoc",
          position: action.data.action_position,
          recommendation_id: action.data.value?.recommendation_id,
          tile_id: action.data.value?.tile_id,
        });
        if (action.data.value?.shim) {
          Glean.pocket.shim.set(action.data.value.shim);
          if (action.data.value.fetchTimestamp) {
            Glean.pocket.fetchTimestamp.set(
              action.data.value.fetchTimestamp * 1000
            );
          }
          if (action.data.value.newtabCreationTimestamp) {
            Glean.pocket.newtabCreationTimestamp.set(
              action.data.value.newtabCreationTimestamp * 1000
            );
          }
          GleanPings.spoc.submit("save");
        }
        break;
    }
  }

  async handleASRouterUserEvent(action) {
    const { ping, pingType } = await this.createASRouterEvent(action);
    if (!pingType) {
      console.error("Unknown ping type for ASRouter telemetry");
      return;
    }

    // Now that the action has become a ping, we can echo it to Glean.
    if (this.telemetryEnabled) {
      lazy.Telemetry.submitGleanPingForPing({ ...ping, pingType });
    }
  }

  /**
   * This function is used by ActivityStreamStorage to report errors
   * trying to access IndexedDB.
   */
  SendASRouterUndesiredEvent(data) {
    this.handleASRouterUserEvent({
      data: { ...data, action: "asrouter_undesired_event" },
    });
  }

  async sendPageTakeoverData() {
    if (this.telemetryEnabled) {
      const value = {};
      let homeAffected = false;
      let newtabCategory = "disabled";
      let homePageCategory = "disabled";

      // Check whether or not about:home and about:newtab are set to a custom URL.
      // If so, classify them.
      if (Services.prefs.getBoolPref("browser.newtabpage.enabled")) {
        newtabCategory = "enabled";
        if (
          lazy.AboutNewTab.newTabURLOverridden &&
          !lazy.AboutNewTab.newTabURL.startsWith("moz-extension://")
        ) {
          value.newtab_url_category = await this._classifySite(
            lazy.AboutNewTab.newTabURL
          );
          newtabCategory = value.newtab_url_category;
        }
      }
      // Check if the newtab page setting is controlled by an extension.
      await lazy.ExtensionSettingsStore.initialize();
      const newtabExtensionInfo = lazy.ExtensionSettingsStore.getSetting(
        "url_overrides",
        "newTabURL"
      );
      if (newtabExtensionInfo && newtabExtensionInfo.id) {
        value.newtab_extension_id = newtabExtensionInfo.id;
        newtabCategory = "extension";
      }

      const homePageURL = lazy.HomePage.get();
      if (
        !["about:home", "about:blank"].includes(homePageURL) &&
        !homePageURL.startsWith("moz-extension://")
      ) {
        value.home_url_category = await this._classifySite(homePageURL);
        homeAffected = true;
        homePageCategory = value.home_url_category;
      }
      const homeExtensionInfo = lazy.ExtensionSettingsStore.getSetting(
        "prefs",
        "homepage_override"
      );
      if (homeExtensionInfo && homeExtensionInfo.id) {
        value.home_extension_id = homeExtensionInfo.id;
        homeAffected = true;
        homePageCategory = "extension";
      }
      if (!homeAffected && !lazy.HomePage.overridden) {
        homePageCategory = "enabled";
      }

      Glean.newtab.newtabCategory.set(newtabCategory);
      Glean.newtab.homepageCategory.set(homePageCategory);
      if (lazy.NimbusFeatures.glean.getVariable("newtabPingEnabled") ?? true) {
        GleanPings.newtab.submit("component_init");
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
      case at.DISCOVERY_STREAM_IMPRESSION_STATS:
        this.handleDiscoveryStreamImpressionStats(
          au.getPortIdOfSender(action),
          action.data
        );
        break;
      case at.DISCOVERY_STREAM_USER_EVENT:
        this.handleDiscoveryStreamUserEvent(action);
        break;
      case at.TELEMETRY_USER_EVENT:
        this.handleUserEvent(action);
        break;
      // The next few action types come from ASRouter, which doesn't use
      // Actions from Actions.jsm, but uses these other custom strings.
      case msg.TOOLBAR_BADGE_TELEMETRY:
      // Intentional fall-through
      case msg.TOOLBAR_PANEL_TELEMETRY:
      // Intentional fall-through
      case msg.MOMENTS_PAGE_TELEMETRY:
      // Intentional fall-through
      case msg.DOORHANGER_TELEMETRY:
      // Intentional fall-through
      case msg.INFOBAR_TELEMETRY:
      // Intentional fall-through
      case msg.SPOTLIGHT_TELEMETRY:
      // Intentional fall-through
      case msg.TOAST_NOTIFICATION_TELEMETRY:
      // Intentional fall-through
      case at.AS_ROUTER_TELEMETRY_USER_EVENT:
        this.handleASRouterUserEvent(action);
        break;
      case at.TOP_SITES_SPONSORED_IMPRESSION_STATS:
        this.handleTopSitesSponsoredImpressionStats(action);
        break;
      case at.TOP_SITES_ORGANIC_IMPRESSION_STATS:
        this.handleTopSitesOrganicImpressionStats(action);
        break;
      case at.UNINIT:
        this.uninit();
        break;
      case at.ABOUT_SPONSORED_TOP_SITES:
        this.handleAboutSponsoredTopSites(action);
        break;
      case at.BLOCK_URL:
        this.handleBlockUrl(action);
        break;
      case at.WALLPAPER_CATEGORY_CLICK:
      case at.WALLPAPER_CLICK:
      case at.WALLPAPERS_FEATURE_HIGHLIGHT_DISMISSED:
      case at.WALLPAPERS_FEATURE_HIGHLIGHT_CTA_CLICKED:
        this.handleWallpaperUserEvent(action);
        break;
      case at.SET_PREF:
        this.handleSetPref(action);
        break;
      case at.WEATHER_IMPRESSION:
      case at.WEATHER_LOAD_ERROR:
      case at.WEATHER_OPEN_PROVIDER_URL:
      case at.WEATHER_LOCATION_DATA_UPDATE:
        this.handleWeatherUserEvent(action);
        break;
    }
  }

  handleSetPref(action) {
    const prefName = action.data.name;

    // TODO: Migrate this event to handleWeatherUserEvent()
    if (prefName === "weather.display") {
      const session = this.sessions.get(au.getPortIdOfSender(action));

      if (!session) {
        return;
      }

      Glean.newtab.weatherChangeDisplay.record({
        newtab_visit_id: session.session_id,
        weather_display_mode: action.data.value,
      });
    }
  }

  handleWeatherUserEvent(action) {
    const session = this.sessions.get(au.getPortIdOfSender(action));

    if (!session) {
      return;
    }

    // Weather specific telemtry events can be added and parsed here.
    switch (action.type) {
      case "WEATHER_IMPRESSION":
        Glean.newtab.weatherImpression.record({
          newtab_visit_id: session.session_id,
        });
        break;
      case "WEATHER_LOAD_ERROR":
        Glean.newtab.weatherLoadError.record({
          newtab_visit_id: session.session_id,
        });
        break;
      case "WEATHER_OPEN_PROVIDER_URL":
        Glean.newtab.weatherOpenProviderUrl.record({
          newtab_visit_id: session.session_id,
        });
        break;
      case "WEATHER_LOCATION_DATA_UPDATE":
        Glean.newtab.weatherLocationSelected.record({
          newtab_visit_id: session.session_id,
        });
        break;
      default:
        break;
    }
  }

  handleWallpaperUserEvent(action) {
    const session = this.sessions.get(au.getPortIdOfSender(action));

    if (!session) {
      return;
    }

    // Wallpaper specific telemtry events can be added and parsed here.
    switch (action.type) {
      case "WALLPAPER_CATEGORY_CLICK":
        Glean.newtab.wallpaperCategoryClick.record({
          newtab_visit_id: session.session_id,
          selected_category: action.data,
        });
        break;
      case "WALLPAPER_CLICK":
        {
          const { data } = action;
          const { selected_wallpaper, hadPreviousWallpaper } = data;
          // if either of the wallpaper prefs are truthy, they had a previous wallpaper
          Glean.newtab.wallpaperClick.record({
            newtab_visit_id: session.session_id,
            selected_wallpaper,
            hadPreviousWallpaper,
          });
        }
        break;
      case "WALLPAPERS_FEATURE_HIGHLIGHT_CTA_CLICKED":
        Glean.newtab.wallpaperHighlightCtaClick.record({
          newtab_visit_id: session.session_id,
        });
        break;
      case "WALLPAPERS_FEATURE_HIGHLIGHT_DISMISSED":
        Glean.newtab.wallpaperHighlightDismissed.record({
          newtab_visit_id: session.session_id,
        });
        break;
      default:
        break;
    }
  }

  handleBlockUrl(action) {
    const session = this.sessions.get(au.getPortIdOfSender(action));
    // TODO: Do we want to not send this unless there's a newtab_visit_id?
    if (!session) {
      return;
    }

    // Despite the action name, this is actually a bulk dismiss action:
    // it can be applied to multiple topsites simultaneously.
    const { data } = action;
    for (const datum of data) {
      if (datum.is_pocket_card) {
        // There is no instrumentation for Pocket dismissals (yet).
        continue;
      }
      const { position, advertiser_name, tile_id, isSponsoredTopSite } = datum;
      Glean.topsites.dismiss.record({
        advertiser_name,
        tile_id,
        newtab_visit_id: session.session_id,
        is_sponsored: !!isSponsoredTopSite,
        position,
      });
    }
  }

  handleAboutSponsoredTopSites(action) {
    const session = this.sessions.get(au.getPortIdOfSender(action));
    const { data } = action;
    const { position, advertiser_name, tile_id } = data;

    if (session) {
      Glean.topsites.showPrivacyClick.record({
        advertiser_name,
        tile_id,
        newtab_visit_id: session.session_id,
        position,
      });
    }
  }

  /**
   * Handle impression stats actions from Discovery Stream.
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

    const { tiles } = data;
    tiles.forEach(tile => {
      Glean.pocket.impression.record({
        newtab_visit_id: session.session_id,
        is_sponsored: tile.type === "spoc",
        position: tile.pos,
        recommendation_id: tile.recommendation_id,
        tile_id: tile.id,
      });
      if (tile.shim) {
        Glean.pocket.shim.set(tile.shim);
        if (tile.fetchTimestamp) {
          Glean.pocket.fetchTimestamp.set(tile.fetchTimestamp * 1000);
        }
        if (data.firstVisibleTimestamp) {
          Glean.pocket.newtabCreationTimestamp.set(
            data.firstVisibleTimestamp * 1000
          );
        }
        GleanPings.spoc.submit("impression");
      }
    });
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
      !lazy.HomePage.overridden &&
      Services.prefs.getIntPref("browser.startup.page") === 1
    ) {
      lazy.AboutNewTab.maybeRecordTopsitesPainted(timestamp);
    }

    Object.assign(session.perf, data);

    if (data.visibility_event_rcvd_ts && !session.newtabOpened) {
      session.newtabOpened = true;
      const source = ONBOARDING_ALLOWED_PAGE_VALUES.includes(session.page)
        ? session.page
        : "other";
      Glean.newtab.opened.record({
        newtab_visit_id: session.session_id,
        source,
      });
    }
  }

  _beginObservingNewtabPingPrefs() {
    Services.prefs.addObserver(ACTIVITY_STREAM_PREF_BRANCH, this);

    for (const pref of Object.keys(NEWTAB_PING_PREFS)) {
      const fullPrefName = ACTIVITY_STREAM_PREF_BRANCH + pref;
      this._setNewtabPrefMetrics(fullPrefName, false);
    }
    Glean.pocket.isSignedIn.set(lazy.pktApi.isUserLoggedIn());

    Services.prefs.addObserver(TOP_SITES_BLOCKED_SPONSORS_PREF, this);
    this._setBlockedSponsorsMetrics();
  }

  _stopObservingNewtabPingPrefs() {
    Services.prefs.removeObserver(ACTIVITY_STREAM_PREF_BRANCH, this);
    Services.prefs.removeObserver(TOP_SITES_BLOCKED_SPONSORS_PREF, this);
  }

  observe(subject, topic, data) {
    if (data === TOP_SITES_BLOCKED_SPONSORS_PREF) {
      this._setBlockedSponsorsMetrics();
    } else {
      this._setNewtabPrefMetrics(data, true);
    }
  }

  _setNewtabPrefMetrics(fullPrefName, isChanged) {
    const pref = fullPrefName.slice(ACTIVITY_STREAM_PREF_BRANCH.length);
    if (!Object.hasOwn(NEWTAB_PING_PREFS, pref)) {
      return;
    }
    const metric = NEWTAB_PING_PREFS[pref];
    switch (Services.prefs.getPrefType(fullPrefName)) {
      case Services.prefs.PREF_BOOL:
        metric.set(Services.prefs.getBoolPref(fullPrefName));
        break;

      case Services.prefs.PREF_INT:
        metric.set(Services.prefs.getIntPref(fullPrefName));
        break;
    }
    if (isChanged) {
      switch (fullPrefName) {
        case `${ACTIVITY_STREAM_PREF_BRANCH}feeds.topsites`:
        case `${ACTIVITY_STREAM_PREF_BRANCH}showSponsoredTopSites`:
          Glean.topsites.prefChanged.record({
            pref_name: fullPrefName,
            new_value: Services.prefs.getBoolPref(fullPrefName),
          });
          break;
      }
    }
  }

  _setBlockedSponsorsMetrics() {
    let blocklist;
    try {
      blocklist = JSON.parse(
        Services.prefs.getStringPref(TOP_SITES_BLOCKED_SPONSORS_PREF, "[]")
      );
    } catch (e) {}
    if (blocklist) {
      Glean.newtab.blockedSponsors.set(blocklist);
    }
  }

  uninit() {
    this._stopObservingNewtabPingPrefs();

    try {
      Services.obs.removeObserver(
        this.browserOpenNewtabStart,
        "browser-open-newtab-start"
      );
    } catch (e) {
      // Operation can fail when uninit is called before
      // init has finished setting up the observer
    }

    // Only uninit if the getter has initialized it
    if (Object.prototype.hasOwnProperty.call(this, "utEvents")) {
      this.utEvents.uninit();
    }

    // TODO: Send any unfinished sessions
  }
}
