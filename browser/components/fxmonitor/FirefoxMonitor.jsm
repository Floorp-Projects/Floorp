/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["FirefoxMonitor"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  EveryWindow: "resource:///modules/EveryWindow.jsm",
  PluralForm: "resource://gre/modules/PluralForm.jsm",
  Preferences: "resource://gre/modules/Preferences.jsm",
  RemoteSettings: "resource://services-settings/remote-settings.js",
  Services: "resource://gre/modules/Services.jsm",
});

const STYLESHEET = "chrome://browser/content/fxmonitor/FirefoxMonitor.css";
const ICON = "chrome://browser/content/fxmonitor/monitor32.svg";

this.FirefoxMonitor = {
  // Map of breached site host -> breach metadata.
  domainMap: new Map(),

  // Reference to the extension object from the WebExtension context.
  // Used for getting URIs for resources packaged in the extension.
  extension: null,

  // Whether we've started observing for the user visiting a breached site.
  observerAdded: false,

  // This is here for documentation, will be redefined to a lazy getter
  // that creates and returns a string bundle in loadStrings().
  strings: null,

  // This is here for documentation, will be redefined to a pref getter
  // using XPCOMUtils.defineLazyPreferenceGetter in init().
  enabled: null,

  kEnabledPref: "extensions.fxmonitor.enabled",

  // This is here for documentation, will be redefined to a pref getter
  // using XPCOMUtils.defineLazyPreferenceGetter in delayedInit().
  // Telemetry event recording is enabled by default.
  // If this pref exists and is true-y, it's disabled.
  telemetryDisabled: null,
  kTelemetryDisabledPref: "extensions.fxmonitor.telemetryDisabled",

  kNotificationID: "fxmonitor",

  // This is here for documentation, will be redefined to a pref getter
  // using XPCOMUtils.defineLazyPreferenceGetter in delayedInit().
  // The value of this property is used as the URL to which the user
  // is directed when they click "Check Firefox Monitor".
  FirefoxMonitorURL: null,
  kFirefoxMonitorURLPref: "extensions.fxmonitor.FirefoxMonitorURL",
  kDefaultFirefoxMonitorURL: "https://monitor.firefox.com",

  // This is here for documentation, will be redefined to a pref getter
  // using XPCOMUtils.defineLazyPreferenceGetter in delayedInit().
  // The pref stores whether the user has seen a breach alert already.
  // The value is used in warnIfNeeded.
  firstAlertShown: null,
  kFirstAlertShownPref: "extensions.fxmonitor.firstAlertShown",

  disable() {
    Preferences.set(this.kEnabledPref, false);
  },

  getString(aKey) {
    return this.strings.GetStringFromName(aKey);
  },

  getFormattedString(aKey, args) {
    return this.strings.formatStringFromName(aKey, args);
  },

  // We used to persist the list of hosts we've already warned the
  // user for in this pref. Now, we check the pref at init and
  // if it has a value, migrate the remembered hosts to content prefs
  // and clear this one.
  kWarnedHostsPref: "extensions.fxmonitor.warnedHosts",
  migrateWarnedHostsIfNeeded() {
    if (!Preferences.isSet(this.kWarnedHostsPref)) {
      return;
    }

    let hosts = [];
    try {
      hosts = JSON.parse(Preferences.get(this.kWarnedHostsPref));
    } catch (ex) {
      // Invalid JSON, nothing to be done.
    }

    let loadContext = Cu.createLoadContext();
    for (let host of hosts) {
      this.rememberWarnedHost(loadContext, host);
    }

    Preferences.reset(this.kWarnedHostsPref);
  },

  init() {
    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "enabled",
      this.kEnabledPref,
      true,
      (pref, oldVal, newVal) => {
        if (newVal) {
          this.startObserving();
        } else {
          this.stopObserving();
        }
      }
    );

    if (this.enabled) {
      this.startObserving();
    }
  },

  // Used to enforce idempotency of delayedInit. delayedInit is
  // called in startObserving() to ensure we load our strings, etc.
  _delayedInited: false,
  async delayedInit() {
    if (this._delayedInited) {
      return;
    }

    this._delayedInited = true;

    XPCOMUtils.defineLazyServiceGetter(
      this,
      "_contentPrefService",
      "@mozilla.org/content-pref/service;1",
      "nsIContentPrefService2"
    );

    this.migrateWarnedHostsIfNeeded();

    // Expire our telemetry on November 1, at which time
    // we should redo data-review.
    let telemetryExpiryDate = new Date(2019, 10, 1); // Month is zero-index
    let today = new Date();
    let expired = today.getTime() > telemetryExpiryDate.getTime();

    Services.telemetry.registerEvents("fxmonitor", {
      interaction: {
        methods: ["interaction"],
        objects: [
          "doorhanger_shown",
          "doorhanger_removed",
          "check_btn",
          "dismiss_btn",
          "never_show_btn",
        ],
        record_on_release: true,
        expired,
      },
    });

    let telemetryEnabled = !Preferences.get(this.kTelemetryDisabledPref);
    Services.telemetry.setEventRecordingEnabled("fxmonitor", telemetryEnabled);

    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "FirefoxMonitorURL",
      this.kFirefoxMonitorURLPref,
      this.kDefaultFirefoxMonitorURL
    );

    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "firstAlertShown",
      this.kFirstAlertShownPref,
      false
    );

    XPCOMUtils.defineLazyGetter(this, "strings", () => {
      return Services.strings.createBundle(
        "chrome://browser/locale/fxmonitor.properties"
      );
    });

    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "telemetryDisabled",
      this.kTelemetryDisabledPref,
      false
    );

    await this.loadBreaches();
  },

  kRemoteSettingsKey: "fxmonitor-breaches",
  async loadBreaches() {
    let populateSites = data => {
      this.domainMap.clear();
      data.forEach(site => {
        if (
          !site.Domain ||
          !site.Name ||
          !site.PwnCount ||
          !site.BreachDate ||
          !site.AddedDate
        ) {
          Cu.reportError(
            `Firefox Monitor: malformed breach entry.\nSite:\n${JSON.stringify(
              site
            )}`
          );
          return;
        }

        try {
          this.domainMap.set(site.Domain, {
            Name: site.Name,
            PwnCount: site.PwnCount,
            Year: new Date(site.BreachDate).getFullYear(),
            AddedDate: site.AddedDate.split("T")[0],
          });
        } catch (e) {
          Cu.reportError(
            `Firefox Monitor: malformed breach entry.\nSite:\n${JSON.stringify(
              site
            )}\nError:\n${e}`
          );
        }
      });
    };

    RemoteSettings(this.kRemoteSettingsKey).on("sync", event => {
      let {
        data: { current },
      } = event;
      populateSites(current);
    });

    let data = await RemoteSettings(this.kRemoteSettingsKey).get();
    if (data && data.length) {
      populateSites(data);
    }
  },

  // nsIWebProgressListener implementation.
  onStateChange(aBrowser, aWebProgress, aRequest, aStateFlags, aStatus) {
    if (
      !(aStateFlags & Ci.nsIWebProgressListener.STATE_STOP) ||
      !aWebProgress.isTopLevel ||
      aWebProgress.isLoadingDocument ||
      !Components.isSuccessCode(aStatus)
    ) {
      return;
    }

    let host;
    try {
      host = Services.eTLD.getBaseDomain(aRequest.URI);
    } catch (e) {
      // If we can't get the host for the URL, it's not one we
      // care about for breach alerts anyway.
      return;
    }

    this.warnIfNeeded(aBrowser, host);
  },

  notificationsByWindow: new WeakMap(),
  panelUIsByWindow: new WeakMap(),

  async startObserving() {
    if (this.observerAdded) {
      return;
    }

    EveryWindow.registerCallback(
      this.kNotificationID,
      win => {
        if (this.notificationsByWindow.has(win)) {
          // We've already set up this window.
          return;
        }

        this.notificationsByWindow.set(win, new Set());

        // Start listening across all tabs! The UI will
        // be set up lazily when we actually need to show
        // a notification.
        this.delayedInit().then(() => {
          win.gBrowser.addTabsProgressListener(this);
        });
      },
      (win, closing) => {
        // If the window is going away, don't bother doing anything.
        if (closing) {
          return;
        }

        let DOMWindowUtils = win.windowUtils;
        DOMWindowUtils.removeSheetUsingURIString(
          STYLESHEET,
          DOMWindowUtils.AUTHOR_SHEET
        );

        if (this.notificationsByWindow.has(win)) {
          this.notificationsByWindow.get(win).forEach(n => {
            n.remove();
          });
          this.notificationsByWindow.delete(win);
        }

        if (this.panelUIsByWindow.has(win)) {
          let doc = win.document;
          doc
            .getElementById(`${this.kNotificationID}-notification-anchor`)
            .remove();
          doc.getElementById(`${this.kNotificationID}-notification`).remove();
          this.panelUIsByWindow.delete(win);
        }

        win.gBrowser.removeTabsProgressListener(this);
      }
    );

    this.observerAdded = true;
  },

  setupPanelUI(win) {
    // Inject our stylesheet.
    let DOMWindowUtils = win.windowUtils;
    DOMWindowUtils.loadSheetUsingURIString(
      STYLESHEET,
      DOMWindowUtils.AUTHOR_SHEET
    );

    // Setup the popup notification stuff. First, the URL bar icon:
    let doc = win.document;
    let notificationBox = doc.getElementById("notification-popup-box");
    // We create a box to use as the anchor, and put an icon image
    // inside it. This way, when we animate the icon, its scale change
    // does not cause the popup notification to bounce due to the anchor
    // point moving.
    let anchorBox = doc.createXULElement("box");
    anchorBox.setAttribute("id", `${this.kNotificationID}-notification-anchor`);
    anchorBox.classList.add("notification-anchor-icon");
    let img = doc.createXULElement("image");
    img.setAttribute("role", "button");
    img.classList.add(`${this.kNotificationID}-icon`);
    img.style.listStyleImage = `url(${ICON})`;
    anchorBox.appendChild(img);
    notificationBox.appendChild(anchorBox);
    img.setAttribute(
      "tooltiptext",
      this.getFormattedString("fxmonitor.anchorIcon.tooltiptext", [
        this.getString("fxmonitor.brandName"),
      ])
    );

    // Now, the popupnotificationcontent:
    let parentElt = doc.defaultView.PopupNotifications.panel.parentNode;
    let pn = doc.createXULElement("popupnotification");
    let pnContent = doc.createXULElement("popupnotificationcontent");
    let panelUI = new PanelUI(doc);
    pnContent.appendChild(panelUI.box);
    pn.appendChild(pnContent);
    pn.setAttribute("id", `${this.kNotificationID}-notification`);
    pn.hidden = true;
    parentElt.appendChild(pn);
    this.panelUIsByWindow.set(win, panelUI);
    return panelUI;
  },

  stopObserving() {
    if (!this.observerAdded) {
      return;
    }

    EveryWindow.unregisterCallback(this.kNotificationID);

    this.observerAdded = false;
  },

  async hostAlreadyWarned(loadContext, host) {
    return new Promise((resolve, reject) => {
      this._contentPrefService.getByDomainAndName(
        host,
        "extensions.fxmonitor.hostAlreadyWarned",
        loadContext,
        {
          handleCompletion: () => resolve(false),
          handleResult: result => resolve(result.value),
        }
      );
    });
  },

  rememberWarnedHost(loadContext, host) {
    this._contentPrefService.set(
      host,
      "extensions.fxmonitor.hostAlreadyWarned",
      true,
      loadContext
    );
  },

  async warnIfNeeded(browser, host) {
    if (
      !this.enabled ||
      !this.domainMap.has(host) ||
      (await this.hostAlreadyWarned(browser.loadContext, host))
    ) {
      return;
    }

    let site = this.domainMap.get(host);

    // We only alert for breaches that were found up to 2 months ago,
    // except for the very first alert we show the user - in which case,
    // we include breaches found in the last three years.
    let breachDateThreshold = new Date();
    if (this.firstAlertShown) {
      breachDateThreshold.setMonth(breachDateThreshold.getMonth() - 2);
    } else {
      breachDateThreshold.setFullYear(breachDateThreshold.getFullYear() - 1);
    }

    if (new Date(site.AddedDate).getTime() < breachDateThreshold.getTime()) {
      return;
    } else if (!this.firstAlertShown) {
      Preferences.set(this.kFirstAlertShownPref, true);
    }

    this.rememberWarnedHost(browser.loadContext, host);

    let doc = browser.ownerDocument;
    let win = doc.defaultView;
    let panelUI = this.panelUIsByWindow.get(win);
    if (!panelUI) {
      panelUI = this.setupPanelUI(win);
    }

    let animatedOnce = false;
    let populatePanel = event => {
      switch (event) {
        case "showing":
          panelUI.refresh(site);
          if (animatedOnce) {
            // If we've already animated once for this site, don't animate again.
            doc
              .getElementById("notification-popup")
              .setAttribute("fxmonitoranimationdone", "true");
            doc
              .getElementById(`${this.kNotificationID}-notification-anchor`)
              .setAttribute("fxmonitoranimationdone", "true");
            break;
          }
          // Make sure we animate if we're coming from another tab that has
          // this attribute set.
          doc
            .getElementById("notification-popup")
            .removeAttribute("fxmonitoranimationdone");
          doc
            .getElementById(`${this.kNotificationID}-notification-anchor`)
            .removeAttribute("fxmonitoranimationdone");
          break;
        case "shown":
          animatedOnce = true;
          break;
        case "removed":
          this.notificationsByWindow
            .get(win)
            .delete(
              win.PopupNotifications.getNotification(
                this.kNotificationID,
                browser
              )
            );
          this.recordEvent("doorhanger_removed");
          break;
      }
    };

    let n = win.PopupNotifications.show(
      browser,
      this.kNotificationID,
      "",
      `${this.kNotificationID}-notification-anchor`,
      panelUI.primaryAction,
      panelUI.secondaryActions,
      {
        persistent: true,
        hideClose: true,
        eventCallback: populatePanel,
        popupIconURL: ICON,
      }
    );

    this.recordEvent("doorhanger_shown");

    this.notificationsByWindow.get(win).add(n);
  },

  recordEvent(aEventName) {
    if (this.telemetryDisabled) {
      return;
    }

    Services.telemetry.recordEvent("fxmonitor", "interaction", aEventName);
  },
};

function PanelUI(doc) {
  this.site = null;
  this.doc = doc;

  let box = doc.createXULElement("vbox");

  let elt = doc.createXULElement("description");
  elt.textContent = this.getString("fxmonitor.popupHeader");
  elt.classList.add("headerText");
  box.appendChild(elt);

  elt = doc.createXULElement("description");
  elt.classList.add("popupText");
  box.appendChild(elt);

  this.box = box;
}

PanelUI.prototype = {
  getString(aKey) {
    return FirefoxMonitor.getString(aKey);
  },

  getFormattedString(aKey, args) {
    return FirefoxMonitor.getFormattedString(aKey, args);
  },

  get brandString() {
    if (this._brandString) {
      return this._brandString;
    }
    return (this._brandString = this.getString("fxmonitor.brandName"));
  },

  getFirefoxMonitorURL: aSiteName => {
    return `${FirefoxMonitor.FirefoxMonitorURL}/?breach=${encodeURIComponent(
      aSiteName
    )}&utm_source=firefox&utm_medium=popup`;
  },

  get primaryAction() {
    if (this._primaryAction) {
      return this._primaryAction;
    }
    return (this._primaryAction = {
      label: this.getFormattedString("fxmonitor.checkButton.label", [
        this.brandString,
      ]),
      accessKey: this.getString("fxmonitor.checkButton.accessKey"),
      callback: () => {
        let win = this.doc.defaultView;
        win.openTrustedLinkIn(
          this.getFirefoxMonitorURL(this.site.Name),
          "tab",
          {}
        );

        FirefoxMonitor.recordEvent("check_btn");
      },
    });
  },

  get secondaryActions() {
    if (this._secondaryActions) {
      return this._secondaryActions;
    }
    return (this._secondaryActions = [
      {
        label: this.getString("fxmonitor.dismissButton.label"),
        accessKey: this.getString("fxmonitor.dismissButton.accessKey"),
        callback: () => {
          FirefoxMonitor.recordEvent("dismiss_btn");
        },
      },
      {
        label: this.getFormattedString("fxmonitor.neverShowButton.label", [
          this.brandString,
        ]),
        accessKey: this.getString("fxmonitor.neverShowButton.accessKey"),
        callback: () => {
          FirefoxMonitor.disable();
          FirefoxMonitor.recordEvent("never_show_btn");
        },
      },
    ]);
  },

  refresh(site) {
    this.site = site;

    let elt = this.box.querySelector(".popupText");

    // If > 100k, the PwnCount is rounded down to the most significant
    // digit and prefixed with "More than".
    // Ex.: 12,345 -> 12,345
    //      234,567 -> More than 200,000
    //      345,678,901 -> More than 300,000,000
    //      4,567,890,123 -> More than 4,000,000,000
    let k100k = 100000;
    let pwnCount = site.PwnCount;
    let stringName = "fxmonitor.popupText";
    if (pwnCount > k100k) {
      let multiplier = 1;
      while (pwnCount >= 10) {
        pwnCount /= 10;
        multiplier *= 10;
      }
      pwnCount = Math.floor(pwnCount) * multiplier;
      stringName = "fxmonitor.popupTextRounded";
    }

    elt.textContent = PluralForm.get(pwnCount, this.getString(stringName))
      .replace("#1", pwnCount.toLocaleString())
      .replace("#2", site.Name)
      .replace("#3", site.Year)
      .replace("#4", this.brandString);
  },
};
