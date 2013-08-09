/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This component listens to notifications for startup, shutdown and session
 * restore, controlling which downloads should be loaded from the database.
 *
 * To avoid affecting startup performance, this component monitors the current
 * session restore state, but defers the actual downloads data manipulation
 * until the Download Manager service is loaded.
 */

"use strict";

////////////////////////////////////////////////////////////////////////////////
//// Globals

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "DownloadsCommon",
                                  "resource:///modules/DownloadsCommon.jsm");
XPCOMUtils.defineLazyServiceGetter(this, "gSessionStartup",
                                   "@mozilla.org/browser/sessionstartup;1",
                                   "nsISessionStartup");

const kObservedTopics = [
  "sessionstore-windows-restored",
  "sessionstore-browser-state-restored",
  "download-manager-initialized",
  "download-manager-change-retention",
  "last-pb-context-exited",
  "browser-lastwindow-close-granted",
  "quit-application",
  "profile-change-teardown",
];

/**
 * CID of our implementation of nsIDownloadManagerUI.
 */
const kDownloadsUICid = Components.ID("{4d99321e-d156-455b-81f7-e7aa2308134f}");

/**
 * Contract ID of the service implementing nsIDownloadManagerUI.
 */
const kDownloadsUIContractId = "@mozilla.org/download-manager-ui;1";

/**
 * CID of the JavaScript implementation of nsITransfer.
 */
const kTransferCid = Components.ID("{1b4c85df-cbdd-4bb6-b04e-613caece083c}");

/**
 * Contract ID of the service implementing nsITransfer.
 */
const kTransferContractId = "@mozilla.org/transfer;1";

////////////////////////////////////////////////////////////////////////////////
//// DownloadsStartup

function DownloadsStartup() { }

DownloadsStartup.prototype = {
  classID: Components.ID("{49507fe5-2cee-4824-b6a3-e999150ce9b8}"),

  _xpcom_factory: XPCOMUtils.generateSingletonFactory(DownloadsStartup),

  //////////////////////////////////////////////////////////////////////////////
  //// nsISupports

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference]),

  //////////////////////////////////////////////////////////////////////////////
  //// nsIObserver

  observe: function DS_observe(aSubject, aTopic, aData)
  {
    switch (aTopic) {
      case "profile-after-change":
        // Override Toolkit's nsIDownloadManagerUI implementation with our own.
        // This must be done at application startup and not in the manifest to
        // ensure that our implementation overrides the original one.
        Components.manager.QueryInterface(Ci.nsIComponentRegistrar)
                          .registerFactory(kDownloadsUICid, "",
                                           kDownloadsUIContractId, null);

        // If the integration preference is enabled, override Toolkit's
        // nsITransfer implementation with the one from the JavaScript API for
        // downloads.  This should be used only by developers while testing new
        // code that uses the JavaScript API, and will eventually be removed
        // when nsIDownloadManager will not be available anymore (bug 851471).
        let useJSTransfer = false;
        try {
          // For performance reasons, we don't want to load the DownloadsCommon
          // module during startup, so we read the preference value directly.
          useJSTransfer =
            Services.prefs.getBoolPref("browser.download.useJSTransfer");
        } catch (ex) { }
        if (useJSTransfer) {
          Components.manager.QueryInterface(Ci.nsIComponentRegistrar)
                            .registerFactory(kTransferCid, "",
                                             kTransferContractId, null);
        } else {
          // The other notifications are handled internally by the JavaScript
          // API for downloads, no need to observe when that API is enabled.
          for (let topic of kObservedTopics) {
            Services.obs.addObserver(this, topic, true);
          }
        }
        break;

      case "sessionstore-windows-restored":
      case "sessionstore-browser-state-restored":
        // Unless there is no saved session, there is a chance that we are
        // starting up after a restart or a crash.  We should check the disk
        // database to see if there are completed downloads to recover and show
        // in the panel, in addition to in-progress downloads.
        if (gSessionStartup.sessionType != Ci.nsISessionStartup.NO_SESSION) {
          this._restoringSession = true;
        }
        this._ensureDataLoaded();
        break;

      case "download-manager-initialized":
        // Don't initialize the JavaScript data and user interface layer if we
        // are initializing the Download Manager service during shutdown.
        if (this._shuttingDown) {
          break;
        }

        // Start receiving events for active and new downloads before we return
        // from this observer function.  We can't defer the execution of this
        // step, to ensure that we don't lose events raised in the meantime.
        DownloadsCommon.initializeAllDataLinks(
                        aSubject.QueryInterface(Ci.nsIDownloadManager));

        this._downloadsServiceInitialized = true;

        // Since this notification is generated during the getService call and
        // we need to get the Download Manager service ourselves, we must post
        // the handler on the event queue to be executed later.
        Services.tm.mainThread.dispatch(this._ensureDataLoaded.bind(this),
                                        Ci.nsIThread.DISPATCH_NORMAL);
        break;

      case "download-manager-change-retention":
        // If we're using the Downloads Panel, we override the retention
        // preference to always retain downloads on completion.
        if (!DownloadsCommon.useToolkitUI) {
          aSubject.QueryInterface(Ci.nsISupportsPRInt32).data = 2;
        }
        break;

      case "browser-lastwindow-close-granted":
        // When using the panel interface, downloads that are already completed
        // should be removed when the last full browser window is closed.  This
        // event is invoked only if the application is not shutting down yet.
        // If the Download Manager service is not initialized, we don't want to
        // initialize it just to clean up completed downloads, because they can
        // be present only in case there was a browser crash or restart.
        if (this._downloadsServiceInitialized &&
            !DownloadsCommon.useToolkitUI) {
          Services.downloads.cleanUp();
        }
        break;

      case "last-pb-context-exited":
        // Similar to the above notification, but for private downloads.
        if (this._downloadsServiceInitialized &&
            !DownloadsCommon.useToolkitUI) {
          Services.downloads.cleanUpPrivate();
        }
        break;

      case "quit-application":
        // When the application is shutting down, we must free all resources in
        // addition to cleaning up completed downloads.  If the Download Manager
        // service is not initialized, we don't want to initialize it just to
        // clean up completed downloads, because they can be present only in
        // case there was a browser crash or restart.
        this._shuttingDown = true;
        if (!this._downloadsServiceInitialized) {
          break;
        }

        DownloadsCommon.terminateAllDataLinks();

        // When using the panel interface, downloads that are already completed
        // should be removed when quitting the application.
        if (!DownloadsCommon.useToolkitUI && aData != "restart") {
          this._cleanupOnShutdown = true;
        }
        break;

      case "profile-change-teardown":
        // If we need to clean up, we must do it synchronously after all the
        // "quit-application" listeners are invoked, so that the Download
        // Manager service has a chance to pause or cancel in-progress downloads
        // before we remove completed downloads from the list.  Note that, since
        // "quit-application" was invoked, we've already exited Private Browsing
        // Mode, thus we are always working on the disk database.
        if (this._cleanupOnShutdown) {
          Services.downloads.cleanUp();
        }

        if (!DownloadsCommon.useToolkitUI) {
          // If we got this far, that means that we finished our first session
          // with the Downloads Panel without crashing. This means that we don't
          // have to force displaying only active downloads on the next startup
          // now.
          this._firstSessionCompleted = true;
        }
        break;
    }
  },

  //////////////////////////////////////////////////////////////////////////////
  //// Private

  /**
   * Indicates whether we're restoring a previous session. This is used by
   * _recoverAllDownloads to determine whether or not we should load and
   * display all downloads data, or restrict it to only the active downloads.
   */
  _restoringSession: false,

  /**
   * Indicates whether the Download Manager service has been initialized.  This
   * flag is required because we want to avoid accessing the service immediately
   * at browser startup.  The service will start when the user first requests a
   * download, or some time after browser startup.
   */
  _downloadsServiceInitialized: false,

  /**
   * True while we are processing the "quit-application" event, and later.
   */
  _shuttingDown: false,

  /**
   * True during shutdown if we need to remove completed downloads.
   */
  _cleanupOnShutdown: false,

  /**
   * True if we should display all downloads, as opposed to just active
   * downloads. We decide to display all downloads if we're restoring a session,
   * or if we're using the Downloads Panel anytime after the first session with
   * it has completed.
   */
  get _recoverAllDownloads() {
    return this._restoringSession ||
           (!DownloadsCommon.useToolkitUI && this._firstSessionCompleted);
  },

  /**
   * True if we've ever completed a session with the Downloads Panel enabled.
   */
  get _firstSessionCompleted() {
    return Services.prefs
                   .getBoolPref("browser.download.panel.firstSessionCompleted");
  },

  set _firstSessionCompleted(aValue) {
    Services.prefs.setBoolPref("browser.download.panel.firstSessionCompleted",
                               aValue);
    return aValue;
  },

  /**
   * Ensures that persistent download data is reloaded at the appropriate time.
   */
  _ensureDataLoaded: function DS_ensureDataLoaded()
  {
    if (!this._downloadsServiceInitialized) {
      return;
    }

    // If the previous session has been already restored, then we ensure that
    // all the downloads are loaded.  Otherwise, we only ensure that the active
    // downloads from the previous session are loaded.
    DownloadsCommon.ensureAllPersistentDataLoaded(!this._recoverAllDownloads);
  }
};

////////////////////////////////////////////////////////////////////////////////
//// Module

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([DownloadsStartup]);
