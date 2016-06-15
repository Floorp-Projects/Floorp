/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- /
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

window.performance.mark('gecko-shell-loadstart');

Cu.import('resource://gre/modules/ContactService.jsm');
Cu.import('resource://gre/modules/AlarmService.jsm');
Cu.import('resource://gre/modules/ActivitiesService.jsm');
Cu.import('resource://gre/modules/NotificationDB.jsm');
Cu.import('resource://gre/modules/Payment.jsm');
Cu.import("resource://gre/modules/AppsUtils.jsm");
Cu.import('resource://gre/modules/UserAgentOverrides.jsm');
Cu.import('resource://gre/modules/Keyboard.jsm');
Cu.import('resource://gre/modules/ErrorPage.jsm');
Cu.import('resource://gre/modules/AlertsHelper.jsm');
Cu.import('resource://gre/modules/SystemUpdateService.jsm');

if (isGonk) {
  Cu.import('resource://gre/modules/NetworkStatsService.jsm');
  Cu.import('resource://gre/modules/ResourceStatsService.jsm');
}

Cu.import('resource://gre/modules/KillSwitchMain.jsm');

// Identity
Cu.import('resource://gre/modules/SignInToWebsite.jsm');
SignInToWebsiteController.init();

Cu.import('resource://gre/modules/FxAccountsMgmtService.jsm');
Cu.import('resource://gre/modules/DownloadsAPI.jsm');
Cu.import('resource://gre/modules/MobileIdentityManager.jsm');
Cu.import('resource://gre/modules/PresentationDeviceInfoManager.jsm');
Cu.import('resource://gre/modules/AboutServiceWorkers.jsm');

XPCOMUtils.defineLazyModuleGetter(this, "SystemAppProxy",
                                  "resource://gre/modules/SystemAppProxy.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Screenshot",
                                  "resource://gre/modules/Screenshot.jsm");

Cu.import('resource://gre/modules/Webapps.jsm');

XPCOMUtils.defineLazyServiceGetter(Services, 'env',
                                   '@mozilla.org/process/environment;1',
                                   'nsIEnvironment');

XPCOMUtils.defineLazyServiceGetter(Services, 'ss',
                                   '@mozilla.org/content/style-sheet-service;1',
                                   'nsIStyleSheetService');

XPCOMUtils.defineLazyServiceGetter(this, 'gSystemMessenger',
                                   '@mozilla.org/system-message-internal;1',
                                   'nsISystemMessagesInternal');

XPCOMUtils.defineLazyGetter(this, "ppmm", function() {
  return Cc["@mozilla.org/parentprocessmessagemanager;1"]
         .getService(Ci.nsIMessageListenerManager);
});

if (isGonk) {
  XPCOMUtils.defineLazyGetter(this, "libcutils", function () {
    Cu.import("resource://gre/modules/systemlibs.js");
    return libcutils;
  });
}

XPCOMUtils.defineLazyServiceGetter(Services, 'captivePortalDetector',
                                  '@mozilla.org/toolkit/captive-detector;1',
                                  'nsICaptivePortalDetector');

if (AppConstants.MOZ_SAFE_BROWSING) {
  XPCOMUtils.defineLazyModuleGetter(this, "SafeBrowsing",
                "resource://gre/modules/SafeBrowsing.jsm");
}

XPCOMUtils.defineLazyModuleGetter(this, "SafeMode",
                                  "resource://gre/modules/SafeMode.jsm");

window.performance.measure('gecko-shell-jsm-loaded', 'gecko-shell-loadstart');

function debug(str) {
  dump(' -*- Shell.js: ' + str + '\n');
}

const once = event => {
  let target = shell.contentBrowser;
  return new Promise((resolve, reject) => {
    target.addEventListener(event, function gotEvent(evt) {
      target.removeEventListener(event, gotEvent, false);
      resolve(evt);
    }, false);
  });
}

function clearCache() {
  let cache = Cc["@mozilla.org/netwerk/cache-storage-service;1"]
                .getService(Ci.nsICacheStorageService);
  cache.clear();
}

function clearCacheAndReload() {
  // Reload the main frame with a cleared cache.
  debug('Reloading ' + shell.contentBrowser.contentWindow.location);
  clearCache();
  shell.contentBrowser.contentWindow.location.reload(true);
  once('mozbrowserlocationchange').then(
    evt => {
      shell.sendEvent(window, "ContentStart");
    });
}

function restart() {
  let appStartup = Cc['@mozilla.org/toolkit/app-startup;1']
                     .getService(Ci.nsIAppStartup);
  appStartup.quit(Ci.nsIAppStartup.eForceQuit | Ci.nsIAppStartup.eRestart);
}

function debugCrashReport(aStr) {
  AppConstants.MOZ_CRASHREPORTER && dump('Crash reporter : ' + aStr);
}

var shell = {

  get CrashSubmit() {
    delete this.CrashSubmit;
    if (AppConstants.MOZ_CRASHREPORTER) {
      Cu.import("resource://gre/modules/CrashSubmit.jsm", this);
      return this.CrashSubmit;
    } else {
      dump('Crash reporter : disabled at build time.');
      return this.CrashSubmit = null;
    }
  },

  onlineForCrashReport: function shell_onlineForCrashReport() {
    let wifiManager = navigator.mozWifiManager;
    let onWifi = (wifiManager &&
                  (wifiManager.connection.status == 'connected'));
    return !Services.io.offline && onWifi;
  },

  reportCrash: function shell_reportCrash(isChrome, aCrashID) {
    let crashID = aCrashID;
    try {
      // For chrome crashes, we want to report the lastRunCrashID.
      if (isChrome) {
        crashID = Cc["@mozilla.org/xre/app-info;1"]
                    .getService(Ci.nsIXULRuntime).lastRunCrashID;
      }
    } catch(e) {
      debugCrashReport('Failed to fetch crash id. Crash ID is "' + crashID
                       + '" Exception: ' + e);
    }

    // Bail if there isn't a valid crashID.
    if (!this.CrashSubmit || !crashID && !this.CrashSubmit.pendingIDs().length) {
      return;
    }

    // purge the queue.
    this.CrashSubmit.pruneSavedDumps();

    // check for environment affecting crash reporting
    let env = Cc["@mozilla.org/process/environment;1"]
                .getService(Ci.nsIEnvironment);
    let shutdown = env.get("MOZ_CRASHREPORTER_SHUTDOWN");
    if (shutdown) {
      let appStartup = Cc["@mozilla.org/toolkit/app-startup;1"]
                         .getService(Ci.nsIAppStartup);
      appStartup.quit(Ci.nsIAppStartup.eForceQuit);
    }

    let noReport = env.get("MOZ_CRASHREPORTER_NO_REPORT");
    if (noReport) {
      return;
    }

    try {
      // Check if we should automatically submit this crash.
      if (Services.prefs.getBoolPref('app.reportCrashes')) {
        this.submitCrash(crashID);
      } else {
        this.deleteCrash(crashID);
      }
    } catch (e) {
      debugCrashReport('Can\'t fetch app.reportCrashes. Exception: ' + e);
    }

    // We can get here if we're just submitting old pending crashes.
    // Check that there's a valid crashID so that we only notify the
    // user if a crash just happened and not when we OOM. Bug 829477
    if (crashID) {
      this.sendChromeEvent({
        type: "handle-crash",
        crashID: crashID,
        chrome: isChrome
      });
    }
  },

  deleteCrash: function shell_deleteCrash(aCrashID) {
    if (aCrashID) {
      debugCrashReport('Deleting pending crash: ' + aCrashID);
      shell.CrashSubmit.delete(aCrashID);
    }
  },

  // this function submit the pending crashes.
  // make sure you are online.
  submitQueuedCrashes: function shell_submitQueuedCrashes() {
    // submit the pending queue.
    let pending = shell.CrashSubmit.pendingIDs();
    for (let crashid of pending) {
      debugCrashReport('Submitting crash: ' + crashid);
      shell.CrashSubmit.submit(crashid);
    }
  },

  // This function submits a crash when we're online.
  submitCrash: function shell_submitCrash(aCrashID) {
    if (this.onlineForCrashReport()) {
      this.submitQueuedCrashes();
      return;
    }

    debugCrashReport('Not online, postponing.');

    Services.obs.addObserver(function observer(subject, topic, state) {
      let network = subject.QueryInterface(Ci.nsINetworkInfo);
      if (network.state == Ci.nsINetworkInfo.NETWORK_STATE_CONNECTED
          && network.type == Ci.nsINetworkInfo.NETWORK_TYPE_WIFI) {
        shell.submitQueuedCrashes();

        Services.obs.removeObserver(observer, topic);
      }
    }, "network-connection-state-changed", false);
  },

  get homeURL() {
    try {
      let homeSrc = Services.env.get('B2G_HOMESCREEN');
      if (homeSrc)
        return homeSrc;
    } catch (e) {}

    return Services.prefs.getCharPref('b2g.system_startup_url');
  },

  get manifestURL() {
    return Services.prefs.getCharPref('b2g.system_manifest_url');
  },

  _started: false,
  hasStarted: function shell_hasStarted() {
    return this._started;
  },

  bootstrap: function() {
    window.performance.mark('gecko-shell-bootstrap');

    // Before anything, check if we want to start in safe mode.
    SafeMode.check(window).then(() => {
      let startManifestURL =
        Cc['@mozilla.org/commandlinehandler/general-startup;1?type=b2gbootstrap']
          .getService(Ci.nsISupports).wrappedJSObject.startManifestURL;

      // If --start-manifest hasn't been specified, we re-use the latest specified manifest.
      // If it's the first launch, we will fallback to b2g.default.start_manifest_url
      if (AppConstants.MOZ_GRAPHENE && !startManifestURL) {
        try {
          startManifestURL = Services.prefs.getCharPref("b2g.system_manifest_url");
        } catch(e) {}
      }

      if (!startManifestURL) {
        try {
          startManifestURL = Services.prefs.getCharPref("b2g.default.start_manifest_url");
        } catch(e) {}
      }

      if (startManifestURL) {
        Cu.import('resource://gre/modules/Bootstraper.jsm');

        if (AppConstants.MOZ_GRAPHENE && Bootstraper.isInstallRequired(startManifestURL)) {
          // Installing the app my take some time. We don't want to keep the
          // native window hidden.
          showInstallScreen();
        }

        Bootstraper.ensureSystemAppInstall(startManifestURL)
                   .then(this.start.bind(this))
                   .catch(Bootstraper.bailout);
      } else {
        this.start();
      }
    });
  },

  start: function shell_start() {
    window.performance.mark('gecko-shell-start');
    this._started = true;

    // This forces the initialization of the cookie service before we hit the
    // network.
    // See bug 810209
    let cookies = Cc["@mozilla.org/cookieService;1"];

    try {
      let cr = Cc["@mozilla.org/xre/app-info;1"]
                 .getService(Ci.nsICrashReporter);
      // Dogfood id. We might want to remove it in the future.
      // see bug 789466
      try {
        let dogfoodId = Services.prefs.getCharPref('prerelease.dogfood.id');
        if (dogfoodId != "") {
          cr.annotateCrashReport("Email", dogfoodId);
        }
      }
      catch (e) { }

      if (isGonk) {
        // Annotate crash report
        let annotations = [ [ "Android_Hardware",     "ro.hardware" ],
                            [ "Android_Device",       "ro.product.device" ],
                            [ "Android_CPU_ABI2",     "ro.product.cpu.abi2" ],
                            [ "Android_CPU_ABI",      "ro.product.cpu.abi" ],
                            [ "Android_Manufacturer", "ro.product.manufacturer" ],
                            [ "Android_Brand",        "ro.product.brand" ],
                            [ "Android_Model",        "ro.product.model" ],
                            [ "Android_Board",        "ro.product.board" ],
          ];

        annotations.forEach(function (element) {
            cr.annotateCrashReport(element[0], libcutils.property_get(element[1]));
          });

        let androidVersion = libcutils.property_get("ro.build.version.sdk") +
                             "(" + libcutils.property_get("ro.build.version.codename") + ")";
        cr.annotateCrashReport("Android_Version", androidVersion);

        SettingsListener.observe("deviceinfo.os", "", function(value) {
          try {
            let cr = Cc["@mozilla.org/xre/app-info;1"]
                       .getService(Ci.nsICrashReporter);
            cr.annotateCrashReport("B2G_OS_Version", value);
          } catch(e) { }
        });
      }
    } catch(e) {
      debugCrashReport('exception: ' + e);
    }

    let homeURL = this.homeURL;
    if (!homeURL) {
      let msg = 'Fatal error during startup: No homescreen found: try setting B2G_HOMESCREEN';
      alert(msg);
      return;
    }
    let manifestURL = this.manifestURL;
    // <html:iframe id="systemapp"
    //              mozbrowser="true" allowfullscreen="true"
    //              style="overflow: hidden; height: 100%; width: 100%; border: none;"
    //              src="data:text/html;charset=utf-8,%3C!DOCTYPE html>%3Cbody style='background:black;'>"/>
    let systemAppFrame =
      document.createElementNS('http://www.w3.org/1999/xhtml', 'html:iframe');
    systemAppFrame.setAttribute('id', 'systemapp');
    systemAppFrame.setAttribute('mozbrowser', 'true');
    systemAppFrame.setAttribute('mozapp', manifestURL);
    systemAppFrame.setAttribute('allowfullscreen', 'true');
    systemAppFrame.setAttribute('src', 'blank.html');
    let container = document.getElementById('container');

    if (AppConstants.platform == 'macosx') {
      // See shell.html
      let hotfix = document.getElementById('placeholder');
      if (hotfix) {
        container.removeChild(hotfix);
      }
    }

    this.contentBrowser = container.appendChild(systemAppFrame);

    let webNav = systemAppFrame.contentWindow
                               .QueryInterface(Ci.nsIInterfaceRequestor)
                               .getInterface(Ci.nsIWebNavigation);
    webNav.sessionHistory = Cc["@mozilla.org/browser/shistory;1"].createInstance(Ci.nsISHistory);

    if (AppConstants.MOZ_GRAPHENE) {
      webNav.QueryInterface(Ci.nsIDocShell).windowDraggingAllowed = true;
    }

    let audioChannels = systemAppFrame.allowedAudioChannels;
    audioChannels && audioChannels.forEach(function(audioChannel) {
      // Set all audio channels as unmuted by default
      // because some audio in System app will be played
      // before AudioChannelService[1] is Gaia is loaded.
      // [1]: https://github.com/mozilla-b2g/gaia/blob/master/apps/system/js/audio_channel_service.js
      audioChannel.setMuted(false);
    });

    // On firefox mulet, shell.html is loaded in a tab
    // and we have to listen on the chrome event handler
    // to catch key events
    let chromeEventHandler = window.QueryInterface(Ci.nsIInterfaceRequestor)
                                   .getInterface(Ci.nsIWebNavigation)
                                   .QueryInterface(Ci.nsIDocShell)
                                   .chromeEventHandler || window;
    // Capture all key events so we can filter out hardware buttons
    // And send them to Gaia via mozChromeEvents.
    // Ideally, hardware buttons wouldn't generate key events at all, or
    // if they did, they would use keycodes that conform to DOM 3 Events.
    // See discussion in https://bugzilla.mozilla.org/show_bug.cgi?id=762362
    chromeEventHandler.addEventListener('keydown', this, true);
    chromeEventHandler.addEventListener('keyup', this, true);

    window.addEventListener('MozApplicationManifest', this);
    window.addEventListener('MozAfterPaint', this);
    window.addEventListener('sizemodechange', this);
    window.addEventListener('unload', this);
    this.contentBrowser.addEventListener('mozbrowserloadstart', this, true);
    this.contentBrowser.addEventListener('mozbrowserscrollviewchange', this, true);
    this.contentBrowser.addEventListener('mozbrowsercaretstatechanged', this);

    CustomEventManager.init();
    WebappsHelper.init();
    UserAgentOverrides.init();
    CaptivePortalLoginHelper.init();

    this.contentBrowser.src = homeURL;
    this._isEventListenerReady = false;

    window.performance.mark('gecko-shell-system-frame-set');

    ppmm.addMessageListener("content-handler", this);
    ppmm.addMessageListener("dial-handler", this);
    ppmm.addMessageListener("sms-handler", this);
    ppmm.addMessageListener("mail-handler", this);
    ppmm.addMessageListener("file-picker", this);

    if (AppConstants.MOZ_SAFE_BROWSING) {
      setTimeout(function() {
        SafeBrowsing.init();
      }, 5000);
    }
  },

  stop: function shell_stop() {
    window.removeEventListener('unload', this);
    window.removeEventListener('keydown', this, true);
    window.removeEventListener('keyup', this, true);
    window.removeEventListener('MozApplicationManifest', this);
    window.removeEventListener('sizemodechange', this);
    this.contentBrowser.removeEventListener('mozbrowserloadstart', this, true);
    this.contentBrowser.removeEventListener('mozbrowserscrollviewchange', this, true);
    this.contentBrowser.removeEventListener('mozbrowsercaretstatechanged', this);
    ppmm.removeMessageListener("content-handler", this);

    UserAgentOverrides.uninit();
  },

  // If this key event represents a hardware button which needs to be send as
  // a message, broadcasts it with the message set to 'xxx-button-press' or
  // 'xxx-button-release'.
  broadcastHardwareKeys: function shell_broadcastHardwareKeys(evt) {
    let type;
    let message;

    let mediaKeys = {
      'MediaTrackNext': 'media-next-track-button',
      'MediaTrackPrevious': 'media-previous-track-button',
      'MediaPause': 'media-pause-button',
      'MediaPlay': 'media-play-button',
      'MediaPlayPause': 'media-play-pause-button',
      'MediaStop': 'media-stop-button',
      'MediaRewind': 'media-rewind-button',
      'MediaFastForward': 'media-fast-forward-button'
    };

    if (evt.keyCode == evt.DOM_VK_F1) {
      type = 'headset-button';
      message = 'headset-button';
    } else if (mediaKeys[evt.key]) {
      type = 'media-button';
      message = mediaKeys[evt.key];
    } else {
      return;
    }

    switch (evt.type) {
      case 'keydown':
        message = message + '-press';
        break;
      case 'keyup':
        message = message + '-release';
        break;
    }

    // Let applications receive the headset button and media key press/release message.
    if (message !== this.lastHardwareButtonMessage) {
      this.lastHardwareButtonMessage = message;
      gSystemMessenger.broadcastMessage(type, message);
    }
  },

  lastHardwareButtonMessage: null, // property for the hack above
  visibleNormalAudioActive: false,

  handleEvent: function shell_handleEvent(evt) {
    function checkReloadKey() {
      if (evt.type !== 'keyup') {
        return false;
      }

      try {
        let key = JSON.parse(Services.prefs.getCharPref('b2g.reload_key'));
        return (evt.keyCode  == key.key   &&
                evt.ctrlKey  == key.ctrl  &&
                evt.altKey   == key.alt   &&
                evt.shiftKey == key.shift &&
                evt.metaKey  == key.meta);
      } catch(e) {
        debug('Failed to get key: ' + e);
      }

      return false;
    }

    let content = this.contentBrowser.contentWindow;
    switch (evt.type) {
      case 'keydown':
      case 'keyup':
        if (checkReloadKey()) {
          clearCacheAndReload();
        } else {
          this.broadcastHardwareKeys(evt);
        }
        break;
      case 'sizemodechange':
        if (window.windowState == window.STATE_MINIMIZED && !this.visibleNormalAudioActive) {
          this.contentBrowser.setVisible(false);
        } else {
          this.contentBrowser.setVisible(true);
        }
        break;
      case 'load':
        if (content.document.location == 'about:blank') {
          return;
        }
        content.removeEventListener('load', this, true);
        this.notifyContentWindowLoaded();
        break;
      case 'mozbrowserloadstart':
        if (content.document.location == 'about:blank') {
          this.contentBrowser.addEventListener('mozbrowserlocationchange', this, true);
          return;
        }

        this.notifyContentStart();
        break;
      case 'mozbrowserlocationchange':
        if (content.document.location == 'about:blank') {
          return;
        }

        this.notifyContentStart();
       break;
      case 'mozbrowserscrollviewchange':
        this.sendChromeEvent({
          type: 'scrollviewchange',
          detail: evt.detail,
        });
        break;
      case 'mozbrowsercaretstatechanged':
        {
          let elt = evt.target;
          let win = elt.ownerDocument.defaultView;
          let offsetX = win.mozInnerScreenX - window.mozInnerScreenX;
          let offsetY = win.mozInnerScreenY - window.mozInnerScreenY;

          let rect = elt.getBoundingClientRect();
          offsetX += rect.left;
          offsetY += rect.top;

          let data = evt.detail;
          data.offsetX = offsetX;
          data.offsetY = offsetY;
          data.sendDoCommandMsg = null;

          shell.sendChromeEvent({
            type: 'caretstatechanged',
            detail: data,
          });
        }
        break;

      case 'MozApplicationManifest':
        try {
          if (!Services.prefs.getBoolPref('browser.cache.offline.enable'))
            return;

          let contentWindow = evt.originalTarget.defaultView;
          let documentElement = contentWindow.document.documentElement;
          if (!documentElement)
            return;

          let manifest = documentElement.getAttribute('manifest');
          if (!manifest)
            return;

          let principal = contentWindow.document.nodePrincipal;
          if (Services.perms.testPermissionFromPrincipal(principal, 'offline-app') == Ci.nsIPermissionManager.UNKNOWN_ACTION) {
            if (Services.prefs.getBoolPref('browser.offline-apps.notify')) {
              // FIXME Bug 710729 - Add a UI for offline cache notifications
              return;
            }
            return;
          }

          Services.perms.addFromPrincipal(principal, 'offline-app',
                                          Ci.nsIPermissionManager.ALLOW_ACTION);

          let documentURI = Services.io.newURI(contentWindow.document.documentURI,
                                               null,
                                               null);
          let manifestURI = Services.io.newURI(manifest, null, documentURI);
          let updateService = Cc['@mozilla.org/offlinecacheupdate-service;1']
                              .getService(Ci.nsIOfflineCacheUpdateService);
          updateService.scheduleUpdate(manifestURI, documentURI, principal, window);
        } catch (e) {
          dump('Error while creating offline cache: ' + e + '\n');
        }
        break;
      case 'MozAfterPaint':
        window.removeEventListener('MozAfterPaint', this);
        // This event should be sent before System app returns with
        // system-message-listener-ready mozContentEvent, because it's on
        // the critical launch path of the app.
        SystemAppProxy._sendCustomEvent('mozChromeEvent', {
          type: 'system-first-paint'
        }, /* noPending */ true);
        break;
      case 'unload':
        this.stop();
        break;
    }
  },

  // Send an event to a specific window, document or element.
  sendEvent: function shell_sendEvent(target, type, details) {
    if (target === this.contentBrowser) {
      // We must ask SystemAppProxy to send the event in this case so
      // that event would be dispatched from frame.contentWindow instead of
      // on the System app frame.
      SystemAppProxy._sendCustomEvent(type, details);
      return;
    }

    let doc = target.document || target.ownerDocument || target;
    let event = doc.createEvent('CustomEvent');
    event.initCustomEvent(type, true, true, details ? details : {});
    target.dispatchEvent(event);
  },

  sendCustomEvent: function shell_sendCustomEvent(type, details) {
    SystemAppProxy._sendCustomEvent(type, details);
  },

  sendChromeEvent: function shell_sendChromeEvent(details) {
    this.sendCustomEvent("mozChromeEvent", details);
  },

  receiveMessage: function shell_receiveMessage(message) {
    var activities = { 'content-handler': { name: 'view', response: null },
                       'dial-handler':    { name: 'dial', response: null },
                       'mail-handler':    { name: 'new',  response: null },
                       'sms-handler':     { name: 'new',  response: null },
                       'file-picker':     { name: 'pick', response: 'file-picked' } };

    if (!(message.name in activities))
      return;

    let data = message.data;
    let activity = activities[message.name];

    let a = new MozActivity({
      name: activity.name,
      data: data
    });

    if (activity.response) {
      a.onsuccess = function() {
        let sender = message.target.QueryInterface(Ci.nsIMessageSender);
        sender.sendAsyncMessage(activity.response, { success: true,
                                                     result:  a.result });
      }
      a.onerror = function() {
        let sender = message.target.QueryInterface(Ci.nsIMessageSender);
        sender.sendAsyncMessage(activity.response, { success: false });
      }
    }
  },

  notifyContentStart: function shell_notifyContentStart() {
    window.performance.mark('gecko-shell-notify-content-start');
    this.contentBrowser.removeEventListener('mozbrowserloadstart', this, true);
    this.contentBrowser.removeEventListener('mozbrowserlocationchange', this, true);

    let content = this.contentBrowser.contentWindow;
    content.addEventListener('load', this, true);

    this.reportCrash(true);

    SystemAppProxy.registerFrame(shell.contentBrowser);

    this.sendEvent(window, 'ContentStart');

    Services.obs.notifyObservers(null, 'content-start', null);

    isGonk && Cu.import('resource://gre/modules/OperatorApps.jsm');

    if (AppConstants.MOZ_GRAPHENE &&
        Services.prefs.getBoolPref("b2g.nativeWindowGeometry.fullscreen")) {
      window.fullScreen = true;
    }

    shell.handleCmdLine();
  },

  handleCmdLine: function() {
    // This isn't supported on devices.
    if (!isGonk) {
      let b2gcmds = Cc["@mozilla.org/commandlinehandler/general-startup;1?type=b2gcmds"]
                      .getService(Ci.nsISupports);
      let args = b2gcmds.wrappedJSObject.cmdLine;
      try {
        // Returns null if -url is not present.
        let url = args.handleFlagWithParam("url", false);
        if (url) {
          this.sendChromeEvent({type: "mozbrowseropenwindow", url});
          args.preventDefault = true;
        }
      } catch(e) {
        // Throws if -url is present with no params.
      }
    }
  },

  // This gets called when window.onload fires on the System app content window,
  // which means things in <html> are parsed and statically referenced <script>s
  // and <script defer>s are loaded and run.
  notifyContentWindowLoaded: function shell_notifyContentWindowLoaded() {
    isGonk && libcutils.property_set('sys.boot_completed', '1');

    // This will cause Gonk Widget to remove boot animation from the screen
    // and reveals the page.
    Services.obs.notifyObservers(null, "browser-ui-startup-complete", "");

    SystemAppProxy.setIsLoaded();
  },

  // This gets called when the content sends us system-message-listener-ready
  // mozContentEvent, OR when an observer message tell us we should consider
  // the content as ready.
  notifyEventListenerReady: function shell_notifyEventListenerReady() {
    if (this._isEventListenerReady) {
      Cu.reportError('shell.js: SystemApp has already been declared as being ready.');
      return;
    }
    this._isEventListenerReady = true;

    if (Services.prefs.getBoolPref('b2g.orientation.animate')) {
      Cu.import('resource://gre/modules/OrientationChangeHandler.jsm');
    }

    SystemAppProxy.setIsReady();
  }
};

Services.obs.addObserver(function onFullscreenOriginChange(subject, topic, data) {
  shell.sendChromeEvent({ type: "fullscreenoriginchange",
                          fullscreenorigin: data });
}, "fullscreen-origin-change", false);

DOMApplicationRegistry.registryReady.then(function () {
  // This event should be sent before System app returns with
  // system-message-listener-ready mozContentEvent, because it's on
  // the critical launch path of the app.
  SystemAppProxy._sendCustomEvent('mozChromeEvent', {
    type: 'webapps-registry-ready'
  }, /* noPending */ true);
});

Services.obs.addObserver(function onBluetoothVolumeChange(subject, topic, data) {
  shell.sendChromeEvent({
    type: "bluetooth-volumeset",
    value: data
  });
}, 'bluetooth-volume-change', false);

Services.obs.addObserver(function(subject, topic, data) {
  shell.sendCustomEvent('mozmemorypressure');
}, 'memory-pressure', false);

Services.obs.addObserver(function(subject, topic, data) {
  shell.notifyEventListenerReady();
}, 'system-message-listener-ready', false);

var permissionMap = new Map([
  ['unknown', Services.perms.UNKNOWN_ACTION],
  ['allow', Services.perms.ALLOW_ACTION],
  ['deny', Services.perms.DENY_ACTION],
  ['prompt', Services.perms.PROMPT_ACTION],
]);
var permissionMapRev = new Map(Array.from(permissionMap.entries()).reverse());

var CustomEventManager = {
  init: function custevt_init() {
    window.addEventListener("ContentStart", (function(evt) {
      let content = shell.contentBrowser.contentWindow;
      content.addEventListener("mozContentEvent", this, false, true);
    }).bind(this), false);
  },

  handleEvent: function custevt_handleEvent(evt) {
    let detail = evt.detail;
    dump('XXX FIXME : Got a mozContentEvent: ' + detail.type + "\n");

    switch(detail.type) {
      case 'webapps-install-granted':
      case 'webapps-install-denied':
      case 'webapps-uninstall-granted':
      case 'webapps-uninstall-denied':
        WebappsHelper.handleEvent(detail);
        break;
      case 'system-message-listener-ready':
        Services.obs.notifyObservers(null, 'system-message-listener-ready', null);
        break;
      case 'captive-portal-login-cancel':
        CaptivePortalLoginHelper.handleEvent(detail);
        break;
      case 'inputmethod-update-layouts':
      case 'inputregistry-add':
      case 'inputregistry-remove':
        KeyboardHelper.handleEvent(detail);
        break;
      case 'copypaste-do-command':
        Services.obs.notifyObservers({ wrappedJSObject: shell.contentBrowser },
                                     'ask-children-to-execute-copypaste-command', detail.cmd);
        break;
      case 'add-permission':
        Services.perms.add(Services.io.newURI(detail.uri, null, null),
                           detail.permissionType, permissionMap.get(detail.permission));
        break;
      case 'remove-permission':
        Services.perms.remove(Services.io.newURI(detail.uri, null, null),
                              detail.permissionType);
        break;
      case 'test-permission':
        let result = Services.perms.testExactPermission(
          Services.io.newURI(detail.uri, null, null), detail.permissionType);
        // Not equal check here because we want to prevent default only if it's not set
        if (result !== permissionMapRev.get(detail.permission)) {
          evt.preventDefault();
        }
        break;
      case 'shutdown-application':
        let appStartup = Cc['@mozilla.org/toolkit/app-startup;1']
                           .getService(Ci.nsIAppStartup);
        appStartup.quit(appStartup.eAttemptQuit);
        break;
      case 'toggle-fullscreen-native-window':
        window.fullScreen = !window.fullScreen;
        Services.prefs.setBoolPref("b2g.nativeWindowGeometry.fullscreen",
                                   window.fullScreen);
        break;
      case 'minimize-native-window':
        window.minimize();
        break;
      case 'clear-cache-and-reload':
        clearCacheAndReload();
        break;
      case 'clear-cache-and-restart':
        clearCache();
        restart();
        break;
      case 'restart':
        restart();
        break;
    }
  }
}

var WebappsHelper = {
  _installers: {},
  _count: 0,

  init: function webapps_init() {
    Services.obs.addObserver(this, "webapps-launch", false);
    Services.obs.addObserver(this, "webapps-ask-install", false);
    Services.obs.addObserver(this, "webapps-ask-uninstall", false);
    Services.obs.addObserver(this, "webapps-close", false);
  },

  registerInstaller: function webapps_registerInstaller(data) {
    let id = "installer" + this._count++;
    this._installers[id] = data;
    return id;
  },

  handleEvent: function webapps_handleEvent(detail) {
    if (!detail || !detail.id)
      return;

    let installer = this._installers[detail.id];
    delete this._installers[detail.id];
    switch (detail.type) {
      case "webapps-install-granted":
        DOMApplicationRegistry.confirmInstall(installer);
        break;
      case "webapps-install-denied":
        DOMApplicationRegistry.denyInstall(installer);
        break;
      case "webapps-uninstall-granted":
        DOMApplicationRegistry.confirmUninstall(installer);
        break;
      case "webapps-uninstall-denied":
        DOMApplicationRegistry.denyUninstall(installer);
        break;
    }
  },

  observe: function webapps_observe(subject, topic, data) {
    let json = JSON.parse(data);
    json.mm = subject;

    let id;

    switch(topic) {
      case "webapps-launch":
        DOMApplicationRegistry.getManifestFor(json.manifestURL).then((aManifest) => {
          if (!aManifest)
            return;

          let manifest = new ManifestHelper(aManifest, json.origin,
                                            json.manifestURL);
          let payload = {
            timestamp: json.timestamp,
            url: manifest.fullLaunchPath(json.startPoint),
            manifestURL: json.manifestURL
          };
          shell.sendCustomEvent("webapps-launch", payload);
        });
        break;
      case "webapps-ask-install":
        id = this.registerInstaller(json);
        shell.sendChromeEvent({
          type: "webapps-ask-install",
          id: id,
          app: json.app
        });
        break;
      case "webapps-ask-uninstall":
        id = this.registerInstaller(json);
        shell.sendChromeEvent({
          type: "webapps-ask-uninstall",
          id: id,
          app: json.app
        });
        break;
      case "webapps-close":
        shell.sendCustomEvent("webapps-close", {
          "manifestURL": json.manifestURL
        });
        break;
    }
  }
}

var KeyboardHelper = {
  handleEvent: function keyboard_handleEvent(detail) {
    switch (detail.type) {
      case 'inputmethod-update-layouts':
        Keyboard.setLayouts(detail.layouts);

        break;
      case 'inputregistry-add':
      case 'inputregistry-remove':
        Keyboard.inputRegistryGlue.returnMessage(detail);

        break;
    }
  }
};

// This is the backend for Gaia's screenshot feature.  Gaia requests a
// screenshot by sending a mozContentEvent with detail.type set to
// 'take-screenshot'.  Then we take a screenshot and send a
// mozChromeEvent with detail.type set to 'take-screenshot-success'
// and detail.file set to the an image/png blob
window.addEventListener('ContentStart', function ss_onContentStart() {
  let content = shell.contentBrowser.contentWindow;
  content.addEventListener('mozContentEvent', function ss_onMozContentEvent(e) {
    if (e.detail.type !== 'take-screenshot')
      return;

    try {
      shell.sendChromeEvent({
        type: 'take-screenshot-success',
        file: Screenshot.get()
      });
    } catch (e) {
      dump('exception while creating screenshot: ' + e + '\n');
      shell.sendChromeEvent({
        type: 'take-screenshot-error',
        error: String(e)
      });
    }
  });
});

(function contentCrashTracker() {
  Services.obs.addObserver(function(aSubject, aTopic, aData) {
      let props = aSubject.QueryInterface(Ci.nsIPropertyBag2);
      if (props.hasKey("abnormal") && props.hasKey("dumpID")) {
        shell.reportCrash(false, props.getProperty("dumpID"));
      }
    },
    "ipc:content-shutdown", false);
})();

var CaptivePortalLoginHelper = {
  init: function init() {
    Services.obs.addObserver(this, 'captive-portal-login', false);
    Services.obs.addObserver(this, 'captive-portal-login-abort', false);
    Services.obs.addObserver(this, 'captive-portal-login-success', false);
  },
  handleEvent: function handleEvent(detail) {
    Services.captivePortalDetector.cancelLogin(detail.id);
  },
  observe: function observe(subject, topic, data) {
    shell.sendChromeEvent(JSON.parse(data));
  }
}

// Listen for crashes submitted through the crash reporter UI.
window.addEventListener('ContentStart', function cr_onContentStart() {
  let content = shell.contentBrowser.contentWindow;
  content.addEventListener("mozContentEvent", function cr_onMozContentEvent(e) {
    if (e.detail.type == "submit-crash" && e.detail.crashID) {
      debugCrashReport("submitting crash at user request ", e.detail.crashID);
      shell.submitCrash(e.detail.crashID);
    } else if (e.detail.type == "delete-crash" && e.detail.crashID) {
      debugCrashReport("deleting crash at user request ", e.detail.crashID);
      shell.deleteCrash(e.detail.crashID);
    }
  });
});

window.addEventListener('ContentStart', function update_onContentStart() {
  Cu.import('resource://gre/modules/WebappsUpdater.jsm');
  WebappsUpdater.handleContentStart(shell);

  if (!AppConstants.MOZ_UPDATER) {
    return;
  }

  let promptCc = Cc["@mozilla.org/updates/update-prompt;1"];
  if (!promptCc) {
    return;
  }

  let updatePrompt = promptCc.createInstance(Ci.nsIUpdatePrompt);
  if (!updatePrompt) {
    return;
  }

  updatePrompt.wrappedJSObject.handleContentStart(shell);
});
/* The "GPSChipOn" is to indicate that GPS engine is turned ON by the modem.
   During this GPS engine is turned ON by the modem, we make the location tracking icon visible to user.
   Once GPS engine is turned OFF, the location icon will disappear.
   If GPS engine is not turned ON by the modem or GPS location service is triggered,
   we let GPS service take over the control of showing the location tracking icon.
   The regular sequence of the geolocation-device-events is: starting-> GPSStarting-> shutdown-> GPSShutdown
*/


(function geolocationStatusTracker() {
  let gGeolocationActive = false;
  let GPSChipOn = false;

  Services.obs.addObserver(function(aSubject, aTopic, aData) {
    let oldState = gGeolocationActive;
    let promptWarning = false;
    switch (aData) {
      case "GPSStarting":
        if (!gGeolocationActive) {
          gGeolocationActive = true;
          GPSChipOn = true;
          promptWarning = true;
        }
        break;
      case "GPSShutdown":
        if (GPSChipOn) {
          gGeolocationActive = false;
          GPSChipOn = false;
        }
        break;
      case "starting":
        gGeolocationActive = true;
        GPSChipOn = false;
        break;
      case "shutdown":
        gGeolocationActive = false;
        break;
    }

    if (gGeolocationActive != oldState) {
      shell.sendChromeEvent({
        type: 'geolocation-status',
        active: gGeolocationActive,
        prompt: promptWarning
      });
    }
}, "geolocation-device-events", false);
})();

(function headphonesStatusTracker() {
  Services.obs.addObserver(function(aSubject, aTopic, aData) {
    shell.sendChromeEvent({
      type: 'headphones-status-changed',
      state: aData
    });
}, "headphones-status-changed", false);
})();

(function audioChannelChangedTracker() {
  Services.obs.addObserver(function(aSubject, aTopic, aData) {
    shell.sendChromeEvent({
      type: 'audio-channel-changed',
      channel: aData
    });
}, "audio-channel-changed", false);
})();

(function defaultVolumeChannelChangedTracker() {
  Services.obs.addObserver(function(aSubject, aTopic, aData) {
    shell.sendChromeEvent({
      type: 'default-volume-channel-changed',
      channel: aData
    });
}, "default-volume-channel-changed", false);
})();

(function visibleAudioChannelChangedTracker() {
  Services.obs.addObserver(function(aSubject, aTopic, aData) {
    shell.sendChromeEvent({
      type: 'visible-audio-channel-changed',
      channel: aData
    });
    shell.visibleNormalAudioActive = (aData == 'normal');
}, "visible-audio-channel-changed", false);
})();

(function recordingStatusTracker() {
  // Recording status is tracked per process with following data structure:
  // {<processId>: {<requestURL>: {isApp: <isApp>,
  //                               count: <N>,
  //                               audioCount: <N>,
  //                               videoCount: <N>}}
  let gRecordingActiveProcesses = {};

  let recordingHandler = function(aSubject, aTopic, aData) {
    let props = aSubject.QueryInterface(Ci.nsIPropertyBag2);
    let processId = (props.hasKey('childID')) ? props.get('childID')
                                              : 'main';
    if (processId && !gRecordingActiveProcesses.hasOwnProperty(processId)) {
      gRecordingActiveProcesses[processId] = {};
    }

    let commandHandler = function (requestURL, command) {
      let currentProcess = gRecordingActiveProcesses[processId];
      let currentActive = currentProcess[requestURL];
      let wasActive = (currentActive['count'] > 0);
      let wasAudioActive = (currentActive['audioCount'] > 0);
      let wasVideoActive = (currentActive['videoCount'] > 0);

      switch (command.type) {
        case 'starting':
          currentActive['count']++;
          currentActive['audioCount'] += (command.isAudio) ? 1 : 0;
          currentActive['videoCount'] += (command.isVideo) ? 1 : 0;
          break;
        case 'shutdown':
          currentActive['count']--;
          currentActive['audioCount'] -= (command.isAudio) ? 1 : 0;
          currentActive['videoCount'] -= (command.isVideo) ? 1 : 0;
          break;
        case 'content-shutdown':
          currentActive['count'] = 0;
          currentActive['audioCount'] = 0;
          currentActive['videoCount'] = 0;
          break;
      }

      if (currentActive['count'] > 0) {
        currentProcess[requestURL] = currentActive;
      } else {
        delete currentProcess[requestURL];
      }

      // We need to track changes if any active state is changed.
      let isActive = (currentActive['count'] > 0);
      let isAudioActive = (currentActive['audioCount'] > 0);
      let isVideoActive = (currentActive['videoCount'] > 0);
      if ((isActive != wasActive) ||
          (isAudioActive != wasAudioActive) ||
          (isVideoActive != wasVideoActive)) {
        shell.sendChromeEvent({
          type: 'recording-status',
          active: isActive,
          requestURL: requestURL,
          isApp: currentActive['isApp'],
          isAudio: isAudioActive,
          isVideo: isVideoActive
        });
      }
    };

    switch (aData) {
      case 'starting':
      case 'shutdown':
        // create page record if it is not existed yet.
        let requestURL = props.get('requestURL');
        if (requestURL &&
            !gRecordingActiveProcesses[processId].hasOwnProperty(requestURL)) {
          gRecordingActiveProcesses[processId][requestURL] = {isApp: props.get('isApp'),
                                                              count: 0,
                                                              audioCount: 0,
                                                              videoCount: 0};
        }
        commandHandler(requestURL, { type: aData,
                                     isAudio: props.get('isAudio'),
                                     isVideo: props.get('isVideo')});
        break;
      case 'content-shutdown':
        // iterate through all the existing active processes
        Object.keys(gRecordingActiveProcesses[processId]).forEach(function(requestURL) {
          commandHandler(requestURL, { type: aData,
                                       isAudio: true,
                                       isVideo: true});
        });
        break;
    }

    // clean up process record if no page record in it.
    if (Object.keys(gRecordingActiveProcesses[processId]).length == 0) {
      delete gRecordingActiveProcesses[processId];
    }
  };
  Services.obs.addObserver(recordingHandler, 'recording-device-events', false);
  Services.obs.addObserver(recordingHandler, 'recording-device-ipc-events', false);

  Services.obs.addObserver(function(aSubject, aTopic, aData) {
    // send additional recording events if content process is being killed
    let processId = aSubject.QueryInterface(Ci.nsIPropertyBag2).get('childID');
    if (gRecordingActiveProcesses.hasOwnProperty(processId)) {
      Services.obs.notifyObservers(aSubject, 'recording-device-ipc-events', 'content-shutdown');
    }
  }, 'ipc:content-shutdown', false);
})();

(function volumeStateTracker() {
  Services.obs.addObserver(function(aSubject, aTopic, aData) {
    shell.sendChromeEvent({
      type: 'volume-state-changed',
      active: (aData == 'Shared')
    });
}, 'volume-state-changed', false);
})();

if (isGonk) {
  // Devices don't have all the same partition size for /cache where we
  // store the http cache.
  (function setHTTPCacheSize() {
    let path = Services.prefs.getCharPref("browser.cache.disk.parent_directory");
    let volumeService = Cc["@mozilla.org/telephony/volume-service;1"]
                          .getService(Ci.nsIVolumeService);

    let stats = volumeService.createOrGetVolumeByPath(path).getStats();

    // We must set the size in KB, and keep a bit of free space.
    let size = Math.floor(stats.totalBytes / 1024) - 1024;

    // keep the default value if it is smaller than the physical partition size.
    let oldSize = Services.prefs.getIntPref("browser.cache.disk.capacity");
    if (size < oldSize) {
      Services.prefs.setIntPref("browser.cache.disk.capacity", size);
    }
  })();

  try {
    let gmpService = Cc["@mozilla.org/gecko-media-plugin-service;1"]
                       .getService(Ci.mozIGeckoMediaPluginChromeService);
    gmpService.addPluginDirectory("/system/b2g/gmp-clearkey/0.1");
  } catch(e) {
    dump("Failed to add clearkey path! " + e + "\n");
  }
}

// Calling this observer will cause a shutdown an a profile reset.
// Use eg. : Services.obs.notifyObservers(null, 'b2g-reset-profile', null);
Services.obs.addObserver(function resetProfile(subject, topic, data) {
  Services.obs.removeObserver(resetProfile, topic);

  // Listening for 'profile-before-change-telemetry' which is late in the
  // shutdown sequence, but still has xpcom access.
  Services.obs.addObserver(function clearProfile(subject, topic, data) {
    Services.obs.removeObserver(clearProfile, topic);
    if (isGonk) {
      let json = Cc['@mozilla.org/file/local;1'].createInstance(Ci.nsIFile);
      json.initWithPath('/system/b2g/webapps/webapps.json');
      let toRemove = json.exists()
        // This is a user build, just rm -r /data/local /data/b2g/mozilla
        ? ['/data/local', '/data/b2g/mozilla']
        // This is an eng build. We clear the profile and a set of files
        // under /data/local.
        : ['/data/b2g/mozilla',
           '/data/local/permissions.sqlite',
           '/data/local/storage',
           '/data/local/OfflineCache'];

      toRemove.forEach(function(dir) {
        try {
          let file = Cc['@mozilla.org/file/local;1'].createInstance(Ci.nsIFile);
          file.initWithPath(dir);
          file.remove(true);
        } catch(e) { dump(e); }
      });
    } else {
      // Desktop builds.
      let profile = Services.dirsvc.get('ProfD', Ci.nsIFile);

      // We don't want to remove everything from the profile, since this
      // would prevent us from starting up.
      let whitelist = ['defaults', 'extensions', 'settings.json',
                       'user.js', 'webapps'];
      let enumerator = profile.directoryEntries;
      while (enumerator.hasMoreElements()) {
        let file = enumerator.getNext().QueryInterface(Ci.nsIFile);
        if (whitelist.indexOf(file.leafName) == -1) {
          file.remove(true);
        }
      }
    }
  },
  'profile-before-change-telemetry', false);

  let appStartup = Cc['@mozilla.org/toolkit/app-startup;1']
                     .getService(Ci.nsIAppStartup);
  appStartup.quit(Ci.nsIAppStartup.eForceQuit);
}, 'b2g-reset-profile', false);

var showInstallScreen;

if (AppConstants.MOZ_GRAPHENE) {
  const restoreWindowGeometry = () => {
    let screenX = Services.prefs.getIntPref("b2g.nativeWindowGeometry.screenX");
    let screenY = Services.prefs.getIntPref("b2g.nativeWindowGeometry.screenY");
    let width = Services.prefs.getIntPref("b2g.nativeWindowGeometry.width");
    let height = Services.prefs.getIntPref("b2g.nativeWindowGeometry.height");

    if (screenX == -1) {
      // Center
      screenX = (screen.width - width) / 2;
      screenY = (screen.height - height) / 2;
    }

    moveTo(screenX, screenY);
    resizeTo(width, height);
  }
  restoreWindowGeometry();

  const saveWindowGeometry = () => {
    window.removeEventListener("unload", saveWindowGeometry);
    Services.prefs.setIntPref("b2g.nativeWindowGeometry.screenX", screenX);
    Services.prefs.setIntPref("b2g.nativeWindowGeometry.screenY", screenY);
    Services.prefs.setIntPref("b2g.nativeWindowGeometry.width", outerWidth);
    Services.prefs.setIntPref("b2g.nativeWindowGeometry.height", outerHeight);
  }
  window.addEventListener("unload", saveWindowGeometry);

  var baseWindow = window.QueryInterface(Ci.nsIInterfaceRequestor)
                         .getInterface(Ci.nsIWebNavigation)
                         .QueryInterface(Ci.nsIDocShellTreeItem)
                         .treeOwner
                         .QueryInterface(Ci.nsIInterfaceRequestor)
                         .getInterface(Ci.nsIBaseWindow);

  const showNativeWindow = () => baseWindow.visibility = true;
  const hideNativeWindow = () => baseWindow.visibility = false;

  showInstallScreen = () => {
    const grapheneStrings =
      Services.strings.createBundle('chrome://b2g-l10n/locale/graphene.properties');
    document.querySelector('#installing > .message').textContent =
      grapheneStrings.GetStringFromName('installing');
    showNativeWindow();
  }

  const hideInstallScreen = () => {
    document.body.classList.add('content-loaded');
  }

  window.addEventListener('ContentStart', () => {
    shell.contentBrowser.contentWindow.addEventListener('load', () => {
      hideInstallScreen();
      showNativeWindow();
    });
  });

  hideNativeWindow();
}
