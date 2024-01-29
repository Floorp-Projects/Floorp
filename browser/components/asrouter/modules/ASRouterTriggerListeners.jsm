/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  AboutReaderParent: "resource:///actors/AboutReaderParent.sys.mjs",
  BrowserUtils: "resource://gre/modules/BrowserUtils.sys.mjs",
  EveryWindow: "resource:///modules/EveryWindow.sys.mjs",
  FeatureCalloutBroker:
    "resource://activity-stream/lib/FeatureCalloutBroker.sys.mjs",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
  clearTimeout: "resource://gre/modules/Timer.sys.mjs",
  setTimeout: "resource://gre/modules/Timer.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "log", () => {
  const { Logger } = ChromeUtils.importESModule(
    "resource://messaging-system/lib/Logger.sys.mjs"
  );
  return new Logger("ASRouterTriggerListeners");
});

const FEW_MINUTES = 15 * 60 * 1000; // 15 mins

function isPrivateWindow(win) {
  return (
    !(win instanceof Ci.nsIDOMWindow) ||
    win.closed ||
    lazy.PrivateBrowsingUtils.isWindowPrivate(win)
  );
}

/**
 * Check current location against the list of allowed hosts
 * Additionally verify for redirects and check original request URL against
 * the list.
 *
 * @returns {object} - {host, url} pair that matched the list of allowed hosts
 */
function checkURLMatch(aLocationURI, { hosts, matchPatternSet }, aRequest) {
  // If checks pass we return a match
  let match;
  try {
    match = { host: aLocationURI.host, url: aLocationURI.spec };
  } catch (e) {
    // nsIURI.host can throw for non-nsStandardURL nsIURIs
    return false;
  }

  // Check current location against allowed hosts
  if (hosts.has(match.host)) {
    return match;
  }

  if (matchPatternSet) {
    if (matchPatternSet.matches(match.url)) {
      return match;
    }
  }

  // Nothing else to check, return early
  if (!aRequest) {
    return false;
  }

  // The original URL at the start of the request
  const originalLocation = aRequest.QueryInterface(Ci.nsIChannel).originalURI;
  // We have been redirected
  if (originalLocation.spec !== aLocationURI.spec) {
    return (
      hosts.has(originalLocation.host) && {
        host: originalLocation.host,
        url: originalLocation.spec,
      }
    );
  }

  return false;
}

function createMatchPatternSet(patterns, flags) {
  try {
    return new MatchPatternSet(new Set(patterns), flags);
  } catch (e) {
    console.error(e);
  }
  return new MatchPatternSet([]);
}

/**
 * A Map from trigger IDs to singleton trigger listeners. Each listener must
 * have idempotent `init` and `uninit` methods.
 */
const ASRouterTriggerListeners = new Map([
  [
    "openArticleURL",
    {
      id: "openArticleURL",
      _initialized: false,
      _triggerHandler: null,
      _hosts: new Set(),
      _matchPatternSet: null,
      readerModeEvent: "Reader:UpdateReaderButton",

      init(triggerHandler, hosts, patterns) {
        if (!this._initialized) {
          this.receiveMessage = this.receiveMessage.bind(this);
          lazy.AboutReaderParent.addMessageListener(this.readerModeEvent, this);
          this._triggerHandler = triggerHandler;
          this._initialized = true;
        }
        if (patterns) {
          this._matchPatternSet = createMatchPatternSet([
            ...(this._matchPatternSet ? this._matchPatternSet.patterns : []),
            ...patterns,
          ]);
        }
        if (hosts) {
          hosts.forEach(h => this._hosts.add(h));
        }
      },

      receiveMessage({ data, target }) {
        if (data && data.isArticle) {
          const match = checkURLMatch(target.currentURI, {
            hosts: this._hosts,
            matchPatternSet: this._matchPatternSet,
          });
          if (match) {
            this._triggerHandler(target, { id: this.id, param: match });
          }
        }
      },

      uninit() {
        if (this._initialized) {
          lazy.AboutReaderParent.removeMessageListener(
            this.readerModeEvent,
            this
          );
          this._initialized = false;
          this._triggerHandler = null;
          this._hosts = new Set();
          this._matchPatternSet = null;
        }
      },
    },
  ],
  [
    "openBookmarkedURL",
    {
      id: "openBookmarkedURL",
      _initialized: false,
      _triggerHandler: null,
      _hosts: new Set(),
      bookmarkEvent: "bookmark-icon-updated",

      init(triggerHandler) {
        if (!this._initialized) {
          Services.obs.addObserver(this, this.bookmarkEvent);
          this._triggerHandler = triggerHandler;
          this._initialized = true;
        }
      },

      observe(subject, topic, data) {
        if (topic === this.bookmarkEvent && data === "starred") {
          const browser = Services.wm.getMostRecentBrowserWindow();
          if (browser) {
            this._triggerHandler(browser.gBrowser.selectedBrowser, {
              id: this.id,
            });
          }
        }
      },

      uninit() {
        if (this._initialized) {
          Services.obs.removeObserver(this, this.bookmarkEvent);
          this._initialized = false;
          this._triggerHandler = null;
          this._hosts = new Set();
        }
      },
    },
  ],
  [
    "frequentVisits",
    {
      id: "frequentVisits",
      _initialized: false,
      _triggerHandler: null,
      _hosts: null,
      _matchPatternSet: null,
      _visits: null,

      init(triggerHandler, hosts = [], patterns) {
        if (!this._initialized) {
          this.onTabSwitch = this.onTabSwitch.bind(this);
          lazy.EveryWindow.registerCallback(
            this.id,
            win => {
              if (!isPrivateWindow(win)) {
                win.addEventListener("TabSelect", this.onTabSwitch);
                win.gBrowser.addTabsProgressListener(this);
              }
            },
            win => {
              if (!isPrivateWindow(win)) {
                win.removeEventListener("TabSelect", this.onTabSwitch);
                win.gBrowser.removeTabsProgressListener(this);
              }
            }
          );
          this._visits = new Map();
          this._initialized = true;
        }
        this._triggerHandler = triggerHandler;
        if (patterns) {
          this._matchPatternSet = createMatchPatternSet([
            ...(this._matchPatternSet ? this._matchPatternSet.patterns : []),
            ...patterns,
          ]);
        }
        if (this._hosts) {
          hosts.forEach(h => this._hosts.add(h));
        } else {
          this._hosts = new Set(hosts); // Clone the hosts to avoid unexpected behaviour
        }
      },

      /* _updateVisits - Record visit timestamps for websites that match `this._hosts` and only
       * if it's been more than FEW_MINUTES since the last visit.
       * @param {string} host - Location host of current selected tab
       * @returns {boolean} - If the new visit has been recorded
       */
      _updateVisits(host) {
        const visits = this._visits.get(host);

        if (visits && Date.now() - visits[0] > FEW_MINUTES) {
          this._visits.set(host, [Date.now(), ...visits]);
          return true;
        }
        if (!visits) {
          this._visits.set(host, [Date.now()]);
          return true;
        }

        return false;
      },

      onTabSwitch(event) {
        if (!event.target.ownerGlobal.gBrowser) {
          return;
        }

        const { gBrowser } = event.target.ownerGlobal;
        const match = checkURLMatch(gBrowser.currentURI, {
          hosts: this._hosts,
          matchPatternSet: this._matchPatternSet,
        });
        if (match) {
          this.triggerHandler(gBrowser.selectedBrowser, match);
        }
      },

      triggerHandler(aBrowser, match) {
        const updated = this._updateVisits(match.host);

        // If the previous visit happend less than FEW_MINUTES ago
        // no updates were made, no need to trigger the handler
        if (!updated) {
          return;
        }

        this._triggerHandler(aBrowser, {
          id: this.id,
          param: match,
          context: {
            // Remapped to {host, timestamp} because JEXL operators can only
            // filter over collections (arrays of objects)
            recentVisits: this._visits
              .get(match.host)
              .map(timestamp => ({ host: match.host, timestamp })),
          },
        });
      },

      onLocationChange(aBrowser, aWebProgress, aRequest, aLocationURI, aFlags) {
        // Some websites trigger redirect events after they finish loading even
        // though the location remains the same. This results in onLocationChange
        // events to be fired twice.
        const isSameDocument = !!(
          aFlags & Ci.nsIWebProgressListener.LOCATION_CHANGE_SAME_DOCUMENT
        );
        if (aWebProgress.isTopLevel && !isSameDocument) {
          const match = checkURLMatch(
            aLocationURI,
            { hosts: this._hosts, matchPatternSet: this._matchPatternSet },
            aRequest
          );
          if (match) {
            this.triggerHandler(aBrowser, match);
          }
        }
      },

      uninit() {
        if (this._initialized) {
          lazy.EveryWindow.unregisterCallback(this.id);

          this._initialized = false;
          this._triggerHandler = null;
          this._hosts = null;
          this._matchPatternSet = null;
          this._visits = null;
        }
      },
    },
  ],

  /**
   * Attach listeners to every browser window to detect location changes, and
   * notify the trigger handler whenever we navigate to a URL with a hostname
   * we're looking for.
   */
  [
    "openURL",
    {
      id: "openURL",
      _initialized: false,
      _triggerHandler: null,
      _hosts: null,
      _matchPatternSet: null,
      _visits: null,

      /*
       * If the listener is already initialised, `init` will replace the trigger
       * handler and add any new hosts to `this._hosts`.
       */
      init(triggerHandler, hosts = [], patterns) {
        if (!this._initialized) {
          this.onLocationChange = this.onLocationChange.bind(this);
          lazy.EveryWindow.registerCallback(
            this.id,
            win => {
              if (!isPrivateWindow(win)) {
                win.gBrowser.addTabsProgressListener(this);
              }
            },
            win => {
              if (!isPrivateWindow(win)) {
                win.gBrowser.removeTabsProgressListener(this);
              }
            }
          );

          this._visits = new Map();
          this._initialized = true;
        }
        this._triggerHandler = triggerHandler;
        if (patterns) {
          this._matchPatternSet = createMatchPatternSet([
            ...(this._matchPatternSet ? this._matchPatternSet.patterns : []),
            ...patterns,
          ]);
        }
        if (this._hosts) {
          hosts.forEach(h => this._hosts.add(h));
        } else {
          this._hosts = new Set(hosts); // Clone the hosts to avoid unexpected behaviour
        }
      },

      uninit() {
        if (this._initialized) {
          lazy.EveryWindow.unregisterCallback(this.id);

          this._initialized = false;
          this._triggerHandler = null;
          this._hosts = null;
          this._matchPatternSet = null;
          this._visits = null;
        }
      },

      onLocationChange(aBrowser, aWebProgress, aRequest, aLocationURI, aFlags) {
        // Some websites trigger redirect events after they finish loading even
        // though the location remains the same. This results in onLocationChange
        // events to be fired twice.
        const isSameDocument = !!(
          aFlags & Ci.nsIWebProgressListener.LOCATION_CHANGE_SAME_DOCUMENT
        );
        if (aWebProgress.isTopLevel && !isSameDocument) {
          const match = checkURLMatch(
            aLocationURI,
            { hosts: this._hosts, matchPatternSet: this._matchPatternSet },
            aRequest
          );
          if (match) {
            let visitsCount = (this._visits.get(match.url) || 0) + 1;
            this._visits.set(match.url, visitsCount);
            this._triggerHandler(aBrowser, {
              id: this.id,
              param: match,
              context: { visitsCount },
            });
          }
        }
      },
    },
  ],

  /**
   * Add an observer notification to notify the trigger handler whenever the user
   * saves or updates a login via the login capture doorhanger.
   */
  [
    "newSavedLogin",
    {
      _initialized: false,
      _triggerHandler: null,

      /**
       * If the listener is already initialised, `init` will replace the trigger
       * handler.
       */
      init(triggerHandler) {
        if (!this._initialized) {
          Services.obs.addObserver(this, "LoginStats:NewSavedPassword");
          Services.obs.addObserver(this, "LoginStats:LoginUpdateSaved");
          this._initialized = true;
        }
        this._triggerHandler = triggerHandler;
      },

      uninit() {
        if (this._initialized) {
          Services.obs.removeObserver(this, "LoginStats:NewSavedPassword");
          Services.obs.removeObserver(this, "LoginStats:LoginUpdateSaved");

          this._initialized = false;
          this._triggerHandler = null;
        }
      },

      observe(aSubject, aTopic, aData) {
        if (aSubject.currentURI.asciiHost === "accounts.firefox.com") {
          // Don't notify about saved logins on the FxA login origin since this
          // trigger is used to promote login Sync and getting a recommendation
          // to enable Sync during the sign up process is a bad UX.
          return;
        }

        switch (aTopic) {
          case "LoginStats:NewSavedPassword": {
            this._triggerHandler(aSubject, {
              id: "newSavedLogin",
              context: { type: "save" },
            });
            break;
          }
          case "LoginStats:LoginUpdateSaved": {
            this._triggerHandler(aSubject, {
              id: "newSavedLogin",
              context: { type: "update" },
            });
            break;
          }
          default: {
            throw new Error(`Unexpected observer notification: ${aTopic}`);
          }
        }
      },
    },
  ],
  [
    "formAutofill",
    {
      id: "formAutofill",
      _initialized: false,
      _triggerHandler: null,
      _triggerDelay: 10000, // 10 second delay before triggering
      _topic: "formautofill-storage-changed",
      _events: ["add", "update", "notifyUsed"] /** @see AutofillRecords */,
      _collections: ["addresses", "creditCards"] /** @see AutofillRecords */,

      init(triggerHandler) {
        if (!this._initialized) {
          Services.obs.addObserver(this, this._topic);
          this._initialized = true;
        }
        this._triggerHandler = triggerHandler;
      },

      uninit() {
        if (this._initialized) {
          Services.obs.removeObserver(this, this._topic);
          this._initialized = false;
          this._triggerHandler = null;
        }
      },

      observe(subject, topic, data) {
        const browser =
          Services.wm.getMostRecentBrowserWindow()?.gBrowser.selectedBrowser;
        if (
          !browser ||
          topic !== this._topic ||
          !subject.wrappedJSObject ||
          // Ignore changes caused by manual edits in the credit card/address
          // managers in about:preferences.
          browser.contentWindow?.gSubDialog?.dialogs.length
        ) {
          return;
        }
        let { sourceSync, collectionName } = subject.wrappedJSObject;
        // Ignore changes from sync and changes to untracked collections.
        if (sourceSync || !this._collections.includes(collectionName)) {
          return;
        }
        if (this._events.includes(data)) {
          let event = data;
          let type = collectionName;
          if (event === "notifyUsed") {
            event = "use";
          }
          if (type === "creditCards") {
            type = "card";
          }
          if (type === "addresses") {
            type = "address";
          }
          lazy.setTimeout(() => {
            if (
              this._initialized &&
              // Make sure the browser still exists and is still selected.
              browser.isConnectedAndReady &&
              browser ===
                Services.wm.getMostRecentBrowserWindow()?.gBrowser
                  .selectedBrowser
            ) {
              this._triggerHandler(browser, {
                id: this.id,
                context: { event, type },
              });
            }
          }, this._triggerDelay);
        }
      },
    },
  ],

  [
    "contentBlocking",
    {
      _initialized: false,
      _triggerHandler: null,
      _events: [],
      _sessionPageLoad: 0,
      onLocationChange: null,

      init(triggerHandler, params, patterns) {
        params.forEach(p => this._events.push(p));

        if (!this._initialized) {
          Services.obs.addObserver(this, "SiteProtection:ContentBlockingEvent");
          Services.obs.addObserver(
            this,
            "SiteProtection:ContentBlockingMilestone"
          );
          this.onLocationChange = this._onLocationChange.bind(this);
          lazy.EveryWindow.registerCallback(
            this.id,
            win => {
              if (!isPrivateWindow(win)) {
                win.gBrowser.addTabsProgressListener(this);
              }
            },
            win => {
              if (!isPrivateWindow(win)) {
                win.gBrowser.removeTabsProgressListener(this);
              }
            }
          );

          this._initialized = true;
        }
        this._triggerHandler = triggerHandler;
      },

      uninit() {
        if (this._initialized) {
          Services.obs.removeObserver(
            this,
            "SiteProtection:ContentBlockingEvent"
          );
          Services.obs.removeObserver(
            this,
            "SiteProtection:ContentBlockingMilestone"
          );
          lazy.EveryWindow.unregisterCallback(this.id);
          this.onLocationChange = null;
          this._initialized = false;
        }
        this._triggerHandler = null;
        this._events = [];
        this._sessionPageLoad = 0;
      },

      observe(aSubject, aTopic, aData) {
        switch (aTopic) {
          case "SiteProtection:ContentBlockingEvent":
            const { browser, host, event } = aSubject.wrappedJSObject;
            if (this._events.filter(e => (e & event) === e).length) {
              this._triggerHandler(browser, {
                id: "contentBlocking",
                param: {
                  host,
                  type: event,
                },
                context: {
                  pageLoad: this._sessionPageLoad,
                },
              });
            }
            break;
          case "SiteProtection:ContentBlockingMilestone":
            if (this._events.includes(aSubject.wrappedJSObject.event)) {
              this._triggerHandler(
                Services.wm.getMostRecentBrowserWindow().gBrowser
                  .selectedBrowser,
                {
                  id: "contentBlocking",
                  context: {
                    pageLoad: this._sessionPageLoad,
                  },
                  param: {
                    type: aSubject.wrappedJSObject.event,
                  },
                }
              );
            }
            break;
        }
      },

      _onLocationChange(
        aBrowser,
        aWebProgress,
        aRequest,
        aLocationURI,
        aFlags
      ) {
        // Some websites trigger redirect events after they finish loading even
        // though the location remains the same. This results in onLocationChange
        // events to be fired twice.
        const isSameDocument = !!(
          aFlags & Ci.nsIWebProgressListener.LOCATION_CHANGE_SAME_DOCUMENT
        );
        if (
          ["http", "https"].includes(aLocationURI.scheme) &&
          aWebProgress.isTopLevel &&
          !isSameDocument
        ) {
          this._sessionPageLoad += 1;
        }
      },
    },
  ],

  [
    "captivePortalLogin",
    {
      id: "captivePortalLogin",
      _initialized: false,
      _triggerHandler: null,

      _shouldShowCaptivePortalVPNPromo() {
        return lazy.BrowserUtils.shouldShowVPNPromo();
      },

      init(triggerHandler) {
        if (!this._initialized) {
          Services.obs.addObserver(this, "captive-portal-login-success");
          this._initialized = true;
        }
        this._triggerHandler = triggerHandler;
      },

      observe(aSubject, aTopic, aData) {
        switch (aTopic) {
          case "captive-portal-login-success":
            const browser = Services.wm.getMostRecentBrowserWindow();
            // The check is here rather than in init because some
            // folks leave their browsers running for a long time,
            // eg from before leaving on a plane trip to after landing
            // in the new destination, and the current region may have
            // changed since init time.
            if (browser && this._shouldShowCaptivePortalVPNPromo()) {
              this._triggerHandler(browser.gBrowser.selectedBrowser, {
                id: this.id,
              });
            }
            break;
        }
      },

      uninit() {
        if (this._initialized) {
          this._triggerHandler = null;
          this._initialized = false;
          Services.obs.removeObserver(this, "captive-portal-login-success");
        }
      },
    },
  ],

  [
    "preferenceObserver",
    {
      id: "preferenceObserver",
      _initialized: false,
      _triggerHandler: null,
      _observedPrefs: [],

      init(triggerHandler, prefs) {
        if (!this._initialized) {
          this._triggerHandler = triggerHandler;
          this._initialized = true;
        }
        prefs.forEach(pref => {
          this._observedPrefs.push(pref);
          Services.prefs.addObserver(pref, this);
        });
      },

      observe(aSubject, aTopic, aData) {
        switch (aTopic) {
          case "nsPref:changed":
            const browser = Services.wm.getMostRecentBrowserWindow();
            if (browser && this._observedPrefs.includes(aData)) {
              this._triggerHandler(browser.gBrowser.selectedBrowser, {
                id: this.id,
                param: {
                  type: aData,
                },
              });
            }
            break;
        }
      },

      uninit() {
        if (this._initialized) {
          this._observedPrefs.forEach(pref =>
            Services.prefs.removeObserver(pref, this)
          );
          this._initialized = false;
          this._triggerHandler = null;
          this._observedPrefs = [];
        }
      },
    },
  ],
  [
    "nthTabClosed",
    {
      id: "nthTabClosed",
      _initialized: false,
      _triggerHandler: null,
      // Number of tabs the user closed this session
      _closedTabs: 0,

      init(triggerHandler) {
        this._triggerHandler = triggerHandler;
        if (!this._initialized) {
          lazy.EveryWindow.registerCallback(
            this.id,
            win => {
              win.addEventListener("TabClose", this);
            },
            win => {
              win.removeEventListener("TabClose", this);
            }
          );
          this._initialized = true;
        }
      },
      handleEvent(event) {
        if (this._initialized) {
          if (!event.target.ownerGlobal.gBrowser) {
            return;
          }
          const { gBrowser } = event.target.ownerGlobal;
          this._closedTabs++;
          this._triggerHandler(gBrowser.selectedBrowser, {
            id: this.id,
            context: { tabsClosedCount: this._closedTabs },
          });
        }
      },
      uninit() {
        if (this._initialized) {
          lazy.EveryWindow.unregisterCallback(this.id);
          this._initialized = false;
          this._triggerHandler = null;
          this._closedTabs = 0;
        }
      },
    },
  ],
  [
    "activityAfterIdle",
    {
      id: "activityAfterIdle",
      _initialized: false,
      _triggerHandler: null,
      _idleService: null,
      // Optimization - only report idle state after one minute of idle time.
      // This represents a minimum idleForMilliseconds of 60000.
      _idleThreshold: 60,
      _idleSince: null,
      _quietSince: null,
      _awaitingVisibilityChange: false,
      // Fire the trigger 2 seconds after activity resumes to ensure user is
      // actively using the browser when it fires.
      _triggerDelay: 2000,
      _triggerTimeout: null,
      // We may get an idle notification immediately after waking from sleep.
      // The idle time in such a case will be the amount of time since the last
      // user interaction, which was before the computer went to sleep. We want
      // to ignore them in that case, so we ignore idle notifications that
      // happen within 1 second of the last wake notification.
      _wakeDelay: 1000,
      _lastWakeTime: null,
      _listenedEvents: ["visibilitychange", "TabClose", "TabAttrModified"],
      // When the OS goes to sleep or the process is suspended, we want to drop
      // the idle time, since the time between sleep and wake is expected to be
      // very long (e.g. overnight). Otherwise, this would trigger on the first
      // activity after waking/resuming, counting sleep as idle time. This
      // basically means each session starts with a fresh idle time.
      _observedTopics: [
        "sleep_notification",
        "suspend_process_notification",
        "wake_notification",
        "resume_process_notification",
        "mac_app_activate",
      ],

      get _isVisible() {
        return [...Services.wm.getEnumerator("navigator:browser")].some(
          win => !win.closed && !win.document?.hidden
        );
      },
      get _soundPlaying() {
        return [...Services.wm.getEnumerator("navigator:browser")].some(win =>
          win.gBrowser?.tabs.some(tab => !tab.closing && tab.soundPlaying)
        );
      },
      init(triggerHandler) {
        this._triggerHandler = triggerHandler;
        // Instantiate this here instead of with a lazy service getter so we can
        // stub it in tests (otherwise we'd have to wait up to 6 minutes for an
        // idle notification in certain test environments).
        if (!this._idleService) {
          this._idleService = Cc[
            "@mozilla.org/widget/useridleservice;1"
          ].getService(Ci.nsIUserIdleService);
        }
        if (
          !this._initialized &&
          !lazy.PrivateBrowsingUtils.permanentPrivateBrowsing
        ) {
          this._idleService.addIdleObserver(this, this._idleThreshold);
          for (let topic of this._observedTopics) {
            Services.obs.addObserver(this, topic);
          }
          lazy.EveryWindow.registerCallback(
            this.id,
            win => {
              for (let ev of this._listenedEvents) {
                win.addEventListener(ev, this);
              }
            },
            win => {
              for (let ev of this._listenedEvents) {
                win.removeEventListener(ev, this);
              }
            }
          );
          if (!this._soundPlaying) {
            this._quietSince = Date.now();
          }
          this._initialized = true;
          this.log("Initialized: ", {
            idleTime: this._idleService.idleTime,
            quietSince: this._quietSince,
          });
        }
      },
      observe(subject, topic, data) {
        if (this._initialized) {
          this.log("Heard observer notification: ", {
            subject,
            topic,
            data,
            idleTime: this._idleService.idleTime,
            idleSince: this._idleSince,
            quietSince: this._quietSince,
            lastWakeTime: this._lastWakeTime,
          });
          switch (topic) {
            case "idle":
              const now = Date.now();
              // If the idle notification is within 1 second of the last wake
              // notification, ignore it. We do this to avoid counting time the
              // computer spent asleep as "idle time"
              const isImmediatelyAfterWake =
                this._lastWakeTime &&
                now - this._lastWakeTime < this._wakeDelay;
              if (!isImmediatelyAfterWake) {
                this._idleSince = now - subject.idleTime;
              }
              break;
            case "active":
              // Trigger when user returns from being idle.
              if (this._isVisible) {
                this._onActive();
                this._idleSince = null;
                this._lastWakeTime = null;
              } else if (this._idleSince) {
                // If the window is not visible, we want to wait until it is
                // visible before triggering.
                this._awaitingVisibilityChange = true;
              }
              break;
            // OS/process notifications
            case "wake_notification":
            case "resume_process_notification":
            case "mac_app_activate":
              this._lastWakeTime = Date.now();
            // Fall through to reset idle time.
            default:
              this._idleSince = null;
          }
        }
      },
      handleEvent(event) {
        if (this._initialized) {
          switch (event.type) {
            case "visibilitychange":
              if (this._awaitingVisibilityChange && this._isVisible) {
                this._onActive();
                this._idleSince = null;
                this._lastWakeTime = null;
                this._awaitingVisibilityChange = false;
              }
              break;
            case "TabAttrModified":
              // Listen for DOMAudioPlayback* events.
              if (!event.detail?.changed?.includes("soundplaying")) {
                break;
              }
            // fall through
            case "TabClose":
              this.log("Tab sound changed: ", {
                event,
                idleTime: this._idleService.idleTime,
                idleSince: this._idleSince,
                quietSince: this._quietSince,
              });
              // Maybe update time if a tab closes with sound playing.
              if (this._soundPlaying) {
                this._quietSince = null;
              } else if (!this._quietSince) {
                this._quietSince = Date.now();
              }
          }
        }
      },
      _onActive() {
        this.log("User is active: ", {
          idleTime: this._idleService.idleTime,
          idleSince: this._idleSince,
          quietSince: this._quietSince,
          lastWakeTime: this._lastWakeTime,
        });
        if (this._idleSince && this._quietSince) {
          const win = Services.wm.getMostRecentBrowserWindow();
          if (win && !isPrivateWindow(win) && !this._triggerTimeout) {
            // Time since the most recent user interaction/audio playback,
            // reported as the number of milliseconds the user has been idle.
            const idleForMilliseconds =
              Date.now() - Math.max(this._idleSince, this._quietSince);
            this._triggerTimeout = lazy.setTimeout(() => {
              this._triggerHandler(win.gBrowser.selectedBrowser, {
                id: this.id,
                context: { idleForMilliseconds },
              });
              this._triggerTimeout = null;
            }, this._triggerDelay);
          }
        }
      },
      uninit() {
        if (this._initialized) {
          this._idleService.removeIdleObserver(this, this._idleThreshold);
          for (let topic of this._observedTopics) {
            Services.obs.removeObserver(this, topic);
          }
          lazy.EveryWindow.unregisterCallback(this.id);
          lazy.clearTimeout(this._triggerTimeout);
          this._triggerTimeout = null;
          this._initialized = false;
          this._triggerHandler = null;
          this._idleSince = null;
          this._quietSince = null;
          this._lastWakeTime = null;
          this._awaitingVisibilityChange = false;
          this.log("Uninitialized");
        }
      },
      log(...args) {
        lazy.log.debug("Idle trigger :>>", ...args);
      },

      QueryInterface: ChromeUtils.generateQI([
        "nsIObserver",
        "nsISupportsWeakReference",
      ]),
    },
  ],
  [
    "cookieBannerDetected",
    {
      id: "cookieBannerDetected",
      _initialized: false,
      _triggerHandler: null,

      init(triggerHandler) {
        this._triggerHandler = triggerHandler;
        if (!this._initialized) {
          lazy.EveryWindow.registerCallback(
            this.id,
            win => {
              win.addEventListener("cookiebannerdetected", this);
            },
            win => {
              win.removeEventListener("cookiebannerdetected", this);
            }
          );
          this._initialized = true;
        }
      },
      handleEvent(event) {
        if (this._initialized) {
          const win = event.target || Services.wm.getMostRecentBrowserWindow();
          if (!win) {
            return;
          }
          this._triggerHandler(win.gBrowser.selectedBrowser, {
            id: this.id,
          });
        }
      },
      uninit() {
        if (this._initialized) {
          lazy.EveryWindow.unregisterCallback(this.id);
          this._initialized = false;
          this._triggerHandler = null;
        }
      },
    },
  ],
  [
    "cookieBannerHandled",
    {
      id: "cookieBannerHandled",
      _initialized: false,
      _triggerHandler: null,

      init(triggerHandler) {
        this._triggerHandler = triggerHandler;
        if (!this._initialized) {
          lazy.EveryWindow.registerCallback(
            this.id,
            win => {
              win.addEventListener("cookiebannerhandled", this);
            },
            win => {
              win.removeEventListener("cookiebannerhandled", this);
            }
          );
          this._initialized = true;
        }
      },
      handleEvent(event) {
        if (this._initialized) {
          const browser =
            event.detail.windowContext.rootFrameLoader?.ownerElement;
          const win = browser?.ownerGlobal;
          // We only want to show messages in the active browser window.
          if (
            win === Services.wm.getMostRecentBrowserWindow() &&
            browser === win.gBrowser.selectedBrowser
          ) {
            this._triggerHandler(browser, { id: this.id });
          }
        }
      },
      uninit() {
        if (this._initialized) {
          lazy.EveryWindow.unregisterCallback(this.id);
          this._initialized = false;
          this._triggerHandler = null;
        }
      },
    },
  ],
  [
    "pdfJsFeatureCalloutCheck",
    {
      id: "pdfJsFeatureCalloutCheck",
      _initialized: false,
      _triggerHandler: null,
      _callouts: new WeakMap(),

      init(triggerHandler) {
        if (!this._initialized) {
          this.onLocationChange = this.onLocationChange.bind(this);
          this.onStateChange = this.onLocationChange;
          lazy.EveryWindow.registerCallback(
            this.id,
            win => {
              this.onBrowserWindow(win);
              win.addEventListener("TabSelect", this);
              win.addEventListener("TabClose", this);
              win.addEventListener("SSTabRestored", this);
              win.gBrowser.addTabsProgressListener(this);
            },
            win => {
              win.removeEventListener("TabSelect", this);
              win.removeEventListener("TabClose", this);
              win.removeEventListener("SSTabRestored", this);
              win.gBrowser.removeTabsProgressListener(this);
            }
          );
          this._initialized = true;
        }
        this._triggerHandler = triggerHandler;
      },

      uninit() {
        if (this._initialized) {
          lazy.EveryWindow.unregisterCallback(this.id);
          this._initialized = false;
          this._triggerHandler = null;
          for (let key of ChromeUtils.nondeterministicGetWeakMapKeys(
            this._callouts
          )) {
            const item = this._callouts.get(key);
            if (item) {
              item.callout.endTour(true);
              item.cleanup();
              this._callouts.delete(key);
            }
          }
        }
      },

      async showFeatureCalloutTour(win, browser, panelId, context) {
        const result = await this._triggerHandler(browser, {
          id: "pdfJsFeatureCalloutCheck",
          context,
        });
        if (result.message.trigger) {
          const callout = lazy.FeatureCalloutBroker.showCustomFeatureCallout(
            {
              win,
              browser,
              pref: {
                name:
                  result.message.content?.tour_pref_name ??
                  "browser.pdfjs.feature-tour",
                defaultValue: result.message.content?.tour_pref_default_value,
              },
              location: "pdfjs",
              theme: { preset: "pdfjs", simulateContent: true },
              cleanup: () => {
                this._callouts.delete(win);
              },
            },
            result.message
          );
          if (callout) {
            callout.panelId = panelId;
            this._callouts.set(win, callout);
          }
        }
      },

      onLocationChange(browser) {
        const tabbrowser = browser.getTabBrowser();
        if (browser !== tabbrowser.selectedBrowser) {
          return;
        }
        const win = tabbrowser.ownerGlobal;
        const tab = tabbrowser.selectedTab;
        const existingCallout = this._callouts.get(win);
        const isPDFJS =
          browser.contentPrincipal.originNoSuffix === "resource://pdf.js";
        if (
          existingCallout &&
          (existingCallout.panelId !== tab.linkedPanel || !isPDFJS)
        ) {
          existingCallout.callout.endTour(true);
          existingCallout.cleanup();
        }
        if (!this._callouts.has(win) && isPDFJS) {
          this.showFeatureCalloutTour(win, browser, tab.linkedPanel, {
            source: "open",
          });
        }
      },

      handleEvent(event) {
        const tab = event.target;
        const win = tab.ownerGlobal;
        const { gBrowser } = win;
        if (!gBrowser) {
          return;
        }
        switch (event.type) {
          case "SSTabRestored":
            if (tab !== gBrowser.selectedTab) {
              return;
            }
          // fall through
          case "TabSelect": {
            const browser = gBrowser.getBrowserForTab(tab);
            const existingCallout = this._callouts.get(win);
            const isPDFJS =
              browser.contentPrincipal.originNoSuffix === "resource://pdf.js";
            if (
              existingCallout &&
              (existingCallout.panelId !== tab.linkedPanel || !isPDFJS)
            ) {
              existingCallout.callout.endTour(true);
              existingCallout.cleanup();
            }
            if (!this._callouts.has(win) && isPDFJS) {
              this.showFeatureCalloutTour(win, browser, tab.linkedPanel, {
                source: "open",
              });
            }
            break;
          }
          case "TabClose": {
            const existingCallout = this._callouts.get(win);
            if (
              existingCallout &&
              existingCallout.panelId === tab.linkedPanel
            ) {
              existingCallout.callout.endTour(true);
              existingCallout.cleanup();
            }
            break;
          }
        }
      },

      onBrowserWindow(win) {
        this.onLocationChange(win.gBrowser.selectedBrowser);
      },
    },
  ],
  [
    "newtabFeatureCalloutCheck",
    {
      id: "newtabFeatureCalloutCheck",
      _initialized: false,
      _triggerHandler: null,
      _callouts: new WeakMap(),

      init(triggerHandler) {
        if (!this._initialized) {
          this.onLocationChange = this.onLocationChange.bind(this);
          this.onStateChange = this.onLocationChange;
          lazy.EveryWindow.registerCallback(
            this.id,
            win => {
              this.onBrowserWindow(win);
              win.addEventListener("TabSelect", this);
              win.addEventListener("TabClose", this);
              win.addEventListener("SSTabRestored", this);
              win.gBrowser.addTabsProgressListener(this);
            },
            win => {
              win.removeEventListener("TabSelect", this);
              win.removeEventListener("TabClose", this);
              win.removeEventListener("SSTabRestored", this);
              win.gBrowser.removeTabsProgressListener(this);
            }
          );
          this._initialized = true;
        }
        this._triggerHandler = triggerHandler;
      },

      uninit() {
        if (this._initialized) {
          lazy.EveryWindow.unregisterCallback(this.id);
          this._initialized = false;
          this._triggerHandler = null;
          for (let key of ChromeUtils.nondeterministicGetWeakMapKeys(
            this._callouts
          )) {
            const item = this._callouts.get(key);
            if (item) {
              item.callout.endTour(true);
              item.cleanup();
              this._callouts.delete(key);
            }
          }
        }
      },

      async showFeatureCalloutTour(win, browser, panelId, context) {
        const result = await this._triggerHandler(browser, {
          id: "newtabFeatureCalloutCheck",
          context,
        });
        if (result.message.trigger) {
          const callout = lazy.FeatureCalloutBroker.showCustomFeatureCallout(
            {
              win,
              browser,
              pref: {
                name:
                  result.message.content?.tour_pref_name ??
                  "browser.newtab.feature-tour",
                defaultValue: result.message.content?.tour_pref_default_value,
              },
              location: "newtab",
              theme: { preset: "newtab", simulateContent: true },
              cleanup: () => {
                this._callouts.delete(win);
              },
            },
            result.message
          );
          if (callout) {
            callout.panelId = panelId;
            this._callouts.set(win, callout);
          }
        }
      },

      onLocationChange(browser) {
        const tabbrowser = browser.getTabBrowser();
        if (browser !== tabbrowser.selectedBrowser) {
          return;
        }
        const win = tabbrowser.ownerGlobal;
        const tab = tabbrowser.selectedTab;
        const existingCallout = this._callouts.get(win);
        const isNewtabOrHome =
          browser.currentURI.spec.startsWith("about:home") ||
          browser.currentURI.spec.startsWith("about:newtab");
        if (
          existingCallout &&
          (existingCallout.panelId !== tab.linkedPanel || !isNewtabOrHome)
        ) {
          existingCallout.callout.endTour(true);
          existingCallout.cleanup();
        }
        if (!this._callouts.has(win) && isNewtabOrHome && tab.linkedPanel) {
          this.showFeatureCalloutTour(win, browser, tab.linkedPanel, {
            source: "open",
          });
        }
      },

      handleEvent(event) {
        const tab = event.target;
        const win = tab.ownerGlobal;
        const { gBrowser } = win;
        if (!gBrowser) {
          return;
        }
        switch (event.type) {
          case "SSTabRestored":
            if (tab !== gBrowser.selectedTab) {
              return;
            }
          // fall through
          case "TabSelect": {
            const browser = gBrowser.getBrowserForTab(tab);
            const existingCallout = this._callouts.get(win);
            const isNewtabOrHome =
              browser.currentURI.spec.startsWith("about:home") ||
              browser.currentURI.spec.startsWith("about:newtab");
            if (
              existingCallout &&
              (existingCallout.panelId !== tab.linkedPanel || !isNewtabOrHome)
            ) {
              existingCallout.callout.endTour(true);
              existingCallout.cleanup();
            }
            if (!this._callouts.has(win) && isNewtabOrHome) {
              this.showFeatureCalloutTour(win, browser, tab.linkedPanel, {
                source: "open",
              });
            }
            break;
          }
          case "TabClose": {
            const existingCallout = this._callouts.get(win);
            if (
              existingCallout &&
              existingCallout.panelId === tab.linkedPanel
            ) {
              existingCallout.callout.endTour(true);
              existingCallout.cleanup();
            }
            break;
          }
        }
      },

      onBrowserWindow(win) {
        this.onLocationChange(win.gBrowser.selectedBrowser);
      },
    },
  ],
]);

const EXPORTED_SYMBOLS = ["ASRouterTriggerListeners"];
