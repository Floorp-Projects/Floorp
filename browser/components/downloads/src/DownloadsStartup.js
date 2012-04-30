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
XPCOMUtils.defineLazyServiceGetter(this, "gPrivateBrowsingService",
                                   "@mozilla.org/privatebrowsing;1",
                                   "nsIPrivateBrowsingService");

const kObservedTopics = [
  "sessionstore-windows-restored",
  "sessionstore-browser-state-restored",
  "download-manager-initialized",
  "download-manager-change-retention",
  "private-browsing-transition-complete",
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
      case "app-startup":
        kObservedTopics.forEach(
          function (topic) Services.obs.addObserver(this, topic, true),
          this);

        // Override Toolkit's nsIDownloadManagerUI implementation with our own.
        // This must be done at application startup and not in the manifest to
        // ensure that our implementation overrides the original one.
        Components.manager.QueryInterface(Ci.nsIComponentRegistrar)
                          .registerFactory(kDownloadsUICid, "",
                                           kDownloadsUIContractId, null);
        break;

      case "sessionstore-windows-restored":
      case "sessionstore-browser-state-restored":
        // Unless there is no saved session, there is a chance that we are
        // starting up after a restart or a crash.  We should check the disk
        // database to see if there are completed downloads to recover and show
        // in the panel, in addition to in-progress downloads.
        if (gSessionStartup.sessionType != Ci.nsISessionStartup.NO_SESSION) {
          this._recoverAllDownloads = true;
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
        DownloadsCommon.data.initializeDataLink(
                             aSubject.QueryInterface(Ci.nsIDownloadManager));

        this._downloadsServiceInitialized = true;

        // Since this notification is generated during the getService call and
        // we need to get the Download Manager service ourselves, we must post
        // the handler on the event queue to be executed later.
        Services.tm.mainThread.dispatch(this._ensureDataLoaded.bind(this),
                                        Ci.nsIThread.DISPATCH_NORMAL);
        break;

      case "download-manager-change-retention":
        // When the panel interface is enabled, we use a different preference to
        // determine whether downloads should be removed from view as soon as
        // they are finished.  We do this to allow proper migration to the new
        // feature when using the same profile on multiple versions of the
        // product (bug 697678).
        if (!DownloadsCommon.useToolkitUI) {
          let removeFinishedDownloads = Services.prefs.getBoolPref(
                            "browser.download.panel.removeFinishedDownloads");
          aSubject.QueryInterface(Ci.nsISupportsPRInt32)
                  .data = removeFinishedDownloads ? 0 : 2;
        }
        break;

      case "private-browsing-transition-complete":
        // Ensure that persistent data is reloaded only when the database
        // connection is available again.
        this._ensureDataLoaded();
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

        DownloadsCommon.data.terminateDataLink();

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
        break;
    }
  },

  //////////////////////////////////////////////////////////////////////////////
  //// Private

  /**
   * Indicates whether we should load all downloads from the previous session,
   * including completed items as well as active downloads.
   */
  _recoverAllDownloads: false,

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
   * Ensures that persistent download data is reloaded at the appropriate time.
   */
  _ensureDataLoaded: function DS_ensureDataLoaded()
  {
    if (!this._downloadsServiceInitialized ||
        gPrivateBrowsingService.privateBrowsingEnabled) {
      return;
    }

    // If the previous session has been already restored, then we ensure that
    // all the downloads are loaded.  Otherwise, we only ensure that the active
    // downloads from the previous session are loaded.
    DownloadsCommon.data.ensurePersistentDataLoaded(!this._recoverAllDownloads);
  }
};

////////////////////////////////////////////////////////////////////////////////
//// Module

const NSGetFactory = XPCOMUtils.generateNSGetFactory([DownloadsStartup]);
