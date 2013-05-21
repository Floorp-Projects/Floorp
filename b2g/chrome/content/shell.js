/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- /
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

Cu.import('resource://gre/modules/ContactService.jsm');
Cu.import('resource://gre/modules/SettingsChangeNotifier.jsm');
#ifdef MOZ_B2G_FM
Cu.import('resource://gre/modules/DOMFMRadioParent.jsm');
#endif
Cu.import('resource://gre/modules/AlarmService.jsm');
Cu.import('resource://gre/modules/ActivitiesService.jsm');
Cu.import('resource://gre/modules/PermissionPromptHelper.jsm');
Cu.import('resource://gre/modules/PushService.jsm');
Cu.import('resource://gre/modules/ObjectWrapper.jsm');
Cu.import('resource://gre/modules/accessibility/AccessFu.jsm');
Cu.import('resource://gre/modules/Payment.jsm');
Cu.import("resource://gre/modules/AppsUtils.jsm");
Cu.import('resource://gre/modules/UserAgentOverrides.jsm');
Cu.import('resource://gre/modules/Keyboard.jsm');
Cu.import('resource://gre/modules/ErrorPage.jsm');
#ifdef MOZ_B2G_RIL
Cu.import('resource://gre/modules/NetworkStatsService.jsm');
#endif

// identity
Cu.import('resource://gre/modules/SignInToWebsite.jsm');
SignInToWebsiteController.init();

XPCOMUtils.defineLazyServiceGetter(Services, 'env',
                                   '@mozilla.org/process/environment;1',
                                   'nsIEnvironment');

XPCOMUtils.defineLazyServiceGetter(Services, 'ss',
                                   '@mozilla.org/content/style-sheet-service;1',
                                   'nsIStyleSheetService');

XPCOMUtils.defineLazyServiceGetter(this, 'gSystemMessenger',
                                   '@mozilla.org/system-message-internal;1',
                                   'nsISystemMessagesInternal');

#ifdef MOZ_WIDGET_GONK
XPCOMUtils.defineLazyServiceGetter(Services, 'audioManager',
                                   '@mozilla.org/telephony/audiomanager;1',
                                   'nsIAudioManager');
#else
Services.audioManager = {
  'masterVolume': 0
};
#endif

XPCOMUtils.defineLazyServiceGetter(Services, 'fm',
                                   '@mozilla.org/focus-manager;1',
                                   'nsIFocusManager');

XPCOMUtils.defineLazyGetter(this, 'DebuggerServer', function() {
  Cu.import('resource://gre/modules/devtools/dbg-server.jsm');
  return DebuggerServer;
});

XPCOMUtils.defineLazyGetter(this, "ppmm", function() {
  return Cc["@mozilla.org/parentprocessmessagemanager;1"]
         .getService(Ci.nsIMessageListenerManager);
});

#ifdef MOZ_WIDGET_GONK
XPCOMUtils.defineLazyGetter(this, "libcutils", function () {
  Cu.import("resource://gre/modules/systemlibs.js");
  return libcutils;
});
#endif

#ifdef MOZ_CAPTIVEDETECT
XPCOMUtils.defineLazyServiceGetter(Services, 'captivePortalDetector',
                                  '@mozilla.org/toolkit/captive-detector;1',
                                  'nsICaptivePortalDetector');
#endif

function getContentWindow() {
  return shell.contentBrowser.contentWindow;
}

function debug(str) {
  dump(' -*- Shell.js: ' + str + '\n');
}

#ifdef MOZ_CRASHREPORTER
function debugCrashReport(aStr) {
  dump('Crash reporter : ' + aStr);
}
#else
function debugCrashReport(aStr) {}
#endif

var shell = {

  get CrashSubmit() {
    delete this.CrashSubmit;
#ifdef MOZ_CRASHREPORTER
    Cu.import("resource://gre/modules/CrashSubmit.jsm", this);
    return this.CrashSubmit;
#else
    dump('Crash reporter : disabled at build time.');
    return this.CrashSubmit = null;
#endif
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
      let network = subject.QueryInterface(Ci.nsINetworkInterface);
      if (network.state == Ci.nsINetworkInterface.NETWORK_STATE_CONNECTED
          && network.type == Ci.nsINetworkInterface.NETWORK_TYPE_WIFI) {
        shell.submitQueuedCrashes();

        Services.obs.removeObserver(observer, topic);
      }
    }, "network-interface-state-changed", false);
  },

  get contentBrowser() {
    delete this.contentBrowser;
    return this.contentBrowser = document.getElementById('homescreen');
  },

  get homeURL() {
    try {
      let homeSrc = Services.env.get('B2G_HOMESCREEN');
      if (homeSrc)
        return homeSrc;
    } catch (e) {}

    return Services.prefs.getCharPref('browser.homescreenURL');
  },

  get manifestURL() {
    return Services.prefs.getCharPref('browser.manifestURL');
  },

  _started: false,
  hasStarted: function shell_hasStarted() {
    return this._started;
  },

  start: function shell_start() {
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

#ifdef MOZ_WIDGET_GONK
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
#endif
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
    // <html:iframe id="homescreen"
    //              mozbrowser="true" allowfullscreen="true"
    //              style="overflow: hidden; -moz-box-flex: 1; border: none;"
    //              src="data:text/html;charset=utf-8,%3C!DOCTYPE html>%3Cbody style='background:black;'>"/>
    let browserFrame =
      document.createElementNS('http://www.w3.org/1999/xhtml', 'html:iframe');
    browserFrame.setAttribute('id', 'homescreen');
    browserFrame.setAttribute('mozbrowser', 'true');
    browserFrame.setAttribute('mozapp', manifestURL);
    browserFrame.setAttribute('allowfullscreen', 'true');
    browserFrame.setAttribute('style', "overflow: hidden; -moz-box-flex: 1; border: none;");
    browserFrame.setAttribute('src', "data:text/html;charset=utf-8,%3C!DOCTYPE html>%3Cbody style='background:black;");
    document.getElementById('shell').appendChild(browserFrame);

    browserFrame.contentWindow
                .QueryInterface(Ci.nsIInterfaceRequestor)
                .getInterface(Ci.nsIWebNavigation)
                .sessionHistory = Cc["@mozilla.org/browser/shistory;1"]
                                    .createInstance(Ci.nsISHistory);

    // Capture all key events so we can filter out hardware buttons
    // And send them to Gaia via mozChromeEvents.
    // Ideally, hardware buttons wouldn't generate key events at all, or
    // if they did, they would use keycodes that conform to DOM 3 Events.
    // See discussion in https://bugzilla.mozilla.org/show_bug.cgi?id=762362
    window.addEventListener('keydown', this, true);
    window.addEventListener('keypress', this, true);
    window.addEventListener('keyup', this, true);
    window.addEventListener('MozApplicationManifest', this);
    window.addEventListener('mozfullscreenchange', this);
    window.addEventListener('sizemodechange', this);
    this.contentBrowser.addEventListener('mozbrowserloadstart', this, true);

    CustomEventManager.init();
    WebappsHelper.init();
    AccessFu.attach(window);
    UserAgentOverrides.init();
    IndexedDBPromptHelper.init();
    CaptivePortalLoginHelper.init();

    // XXX could factor out into a settings->pref map.  Not worth it yet.
    SettingsListener.observe("debug.fps.enabled", false, function(value) {
      Services.prefs.setBoolPref("layers.acceleration.draw-fps", value);
    });
    SettingsListener.observe("debug.paint-flashing.enabled", false, function(value) {
      Services.prefs.setBoolPref("nglayout.debug.paint_flashing", value);
    });

    this.contentBrowser.src = homeURL;
    this.isHomeLoaded = false;

    ppmm.addMessageListener("content-handler", this);
    ppmm.addMessageListener("dial-handler", this);
    ppmm.addMessageListener("sms-handler", this);
    ppmm.addMessageListener("mail-handler", this);
    ppmm.addMessageListener("app-notification-send", AlertsHelper);
    ppmm.addMessageListener("file-picker", this);
  },

  stop: function shell_stop() {
    window.removeEventListener('keydown', this, true);
    window.removeEventListener('keypress', this, true);
    window.removeEventListener('keyup', this, true);
    window.removeEventListener('MozApplicationManifest', this);
    window.removeEventListener('mozfullscreenchange', this);
    window.removeEventListener('sizemodechange', this);
    this.contentBrowser.removeEventListener('mozbrowserloadstart', this, true);
    ppmm.removeMessageListener("content-handler", this);
    if (this.timer) {
      this.timer.cancel();
      this.timer = null;
    }

#ifndef MOZ_WIDGET_GONK
    delete Services.audioManager;
#endif
    UserAgentOverrides.uninit();
    IndexedDBPromptHelper.uninit();
  },

  // If this key event actually represents a hardware button, filter it here
  // and send a mozChromeEvent with detail.type set to xxx-button-press or
  // xxx-button-release instead.
  filterHardwareKeys: function shell_filterHardwareKeys(evt) {
    var type;
    switch (evt.keyCode) {
      case evt.DOM_VK_HOME:         // Home button
        type = 'home-button';
        break;
      case evt.DOM_VK_SLEEP:        // Sleep button
      case evt.DOM_VK_END:          // On desktop we don't have a sleep button
        type = 'sleep-button';
        break;
      case evt.DOM_VK_PAGE_UP:      // Volume up button
        type = 'volume-up-button';
        break;
      case evt.DOM_VK_PAGE_DOWN:    // Volume down button
        type = 'volume-down-button';
        break;
      case evt.DOM_VK_ESCAPE:       // Back button (should be disabled)
        type = 'back-button';
        break;
      case evt.DOM_VK_CONTEXT_MENU: // Menu button
        type = 'menu-button';
        break;
      case evt.DOM_VK_F1: // headset button
        type = 'headset-button';
        break;
      default:                      // Anything else is a real key
        return;  // Don't filter it at all; let it propagate to Gaia
    }

    // If we didn't return, then the key event represents a hardware key
    // and we need to prevent it from propagating to Gaia
    evt.stopImmediatePropagation();
    evt.preventDefault(); // Prevent keypress events (when #501496 is fixed).

    // If it is a key down or key up event, we send a chrome event to Gaia.
    // If it is a keypress event we just ignore it.
    switch (evt.type) {
      case 'keydown':
        type = type + '-press';
        break;
      case 'keyup':
        type = type + '-release';
        break;
      case 'keypress':
        return;
    }

    // Let applications receive the headset button key press/release event.
    if (evt.keyCode == evt.DOM_VK_F1 && type !== this.lastHardwareButtonEventType) {
      this.lastHardwareButtonEventType = type;
      gSystemMessenger.broadcastMessage('headset-button', type);
      return;
    }

    // On my device, the physical hardware buttons (sleep and volume)
    // send multiple events (press press release release), but the
    // soft home button just sends one.  This hack is to manually
    // "debounce" the keys. If the type of this event is the same as
    // the type of the last one, then don't send it.  We'll never send
    // two presses or two releases in a row.
    // FIXME: https://bugzilla.mozilla.org/show_bug.cgi?id=761067
    if (type !== this.lastHardwareButtonEventType) {
      this.lastHardwareButtonEventType = type;
      this.sendChromeEvent({type: type});
    }
  },

  lastHardwareButtonEventType: null, // property for the hack above
  needBufferSysMsgs: true,
  bufferedSysMsgs: [],
  timer: null,
  visibleNormalAudioActive: false,

  handleEvent: function shell_handleEvent(evt) {
    let content = this.contentBrowser.contentWindow;
    switch (evt.type) {
      case 'keydown':
      case 'keyup':
      case 'keypress':
        this.filterHardwareKeys(evt);
        break;
      case 'mozfullscreenchange':
        // When the screen goes fullscreen make sure to set the focus to the
        // main window so noboby can prevent the ESC key to get out fullscreen
        // mode
        if (document.mozFullScreen)
          Services.fm.focusedWindow = window;
        break;
      case 'sizemodechange':
        if (window.windowState == window.STATE_MINIMIZED && !this.visibleNormalAudioActive) {
          this.contentBrowser.setVisible(false);
        } else {
          this.contentBrowser.setVisible(true);
        }
        break;
      case 'mozbrowserloadstart':
        if (content.document.location == 'about:blank')
          return;

        this.contentBrowser.removeEventListener('mozbrowserloadstart', this, true);

        this.reportCrash(true);

        let chromeWindow = window.QueryInterface(Ci.nsIDOMChromeWindow);
        chromeWindow.browserDOMWindow = new nsBrowserAccess();

        Cu.import('resource://gre/modules/Webapps.jsm');
        DOMApplicationRegistry.allAppsLaunchable = true;

        this.sendEvent(window, 'ContentStart');

        content.addEventListener('load', function shell_homeLoaded() {
          content.removeEventListener('load', shell_homeLoaded);
          shell.isHomeLoaded = true;

#ifdef MOZ_WIDGET_GONK
          libcutils.property_set('sys.boot_completed', '1');
#endif

          Services.obs.notifyObservers(null, "browser-ui-startup-complete", "");

          if ('pendingChromeEvents' in shell) {
            shell.pendingChromeEvents.forEach((shell.sendChromeEvent).bind(shell));
          }
          delete shell.pendingChromeEvents;
        });

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

          let manifestURI = Services.io.newURI(manifest, null, documentURI);
          let updateService = Cc['@mozilla.org/offlinecacheupdate-service;1']
                              .getService(Ci.nsIOfflineCacheUpdateService);
          updateService.scheduleUpdate(manifestURI, documentURI, window);
        } catch (e) {
          dump('Error while creating offline cache: ' + e + '\n');
        }
        break;
    }
  },

  sendEvent: function shell_sendEvent(content, type, details) {
    let event = content.document.createEvent('CustomEvent');
    event.initCustomEvent(type, true, true, details ? details : {});
    content.dispatchEvent(event);
  },

  sendChromeEvent: function shell_sendChromeEvent(details) {
    if (!this.isHomeLoaded) {
      if (!('pendingChromeEvents' in this)) {
        this.pendingChromeEvents = [];
      }

      this.pendingChromeEvents.push(details);
      return;
    }

    this.sendEvent(getContentWindow(), "mozChromeEvent",
                   ObjectWrapper.wrap(details, getContentWindow()));
  },

  sendSystemMessage: function shell_sendSystemMessage(msg) {
    let origin = Services.io.newURI(msg.manifest, null, null).prePath;
    this.sendChromeEvent({
      type: 'open-app',
      url: msg.uri,
      manifestURL: msg.manifest,
      isActivity: (msg.type == 'activity'),
      target: msg.target,
      expectingSystemMessage: true
    });
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
  }
};

function nsBrowserAccess() {
}

nsBrowserAccess.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIBrowserDOMWindow]),

  openURI: function openURI(uri, opener, where, context) {
    // TODO This should be replaced by an 'open-browser-window' intent
    let content = shell.contentBrowser.contentWindow;
    let contentWindow = content.wrappedJSObject;
    if (!('getApplicationManager' in contentWindow))
      return null;

    let applicationManager = contentWindow.getApplicationManager();
    if (!applicationManager)
      return null;

    let url = uri ? uri.spec : 'about:blank';
    let window = applicationManager.launch(url, where);
    return window.contentWindow;
  },

  openURIInFrame: function openURIInFrame(uri, opener, where, context) {
    throw new Error('Not Implemented');
  },

  isTabContentWindow: function isTabContentWindow(contentWindow) {
    return contentWindow == window;
  }
};

// Listen for system messages and relay them to Gaia.
Services.obs.addObserver(function onSystemMessage(subject, topic, data) {
  let msg = JSON.parse(data);
  // Buffer non-activity messages until content starts to load for 10 seconds.
  // We'll revisit this later if new kind of messages don't need to be cached.
  if (shell.needBufferSysMsgs && msg.type !== 'activity') {
    shell.bufferedSysMsgs.push(msg);
    return;
  }
  shell.sendSystemMessage(msg);
}, 'system-messages-open-app', false);

Services.obs.addObserver(function(aSubject, aTopic, aData) {
  shell.sendChromeEvent({ type: "fullscreenoriginchange",
                          fullscreenorigin: aData });
}, "fullscreen-origin-change", false);

Services.obs.addObserver(function onWebappsStart(subject, topic, data) {
  shell.sendChromeEvent({ type: 'webapps-registry-start' });
}, 'webapps-registry-start', false);

Services.obs.addObserver(function onWebappsReady(subject, topic, data) {
  shell.sendChromeEvent({ type: 'webapps-registry-ready' });
}, 'webapps-registry-ready', false);

Services.obs.addObserver(function onBluetoothVolumeChange(subject, topic, data) {
  shell.sendChromeEvent({
    type: "bluetooth-volumeset",
    value: data
  });
}, 'bluetooth-volume-change', false);

var CustomEventManager = {
  init: function custevt_init() {
    window.addEventListener("ContentStart", (function(evt) {
      let content = shell.contentBrowser.contentWindow;
      content.addEventListener("mozContentEvent", this, false, true);

      // After content starts to load for 10 seconds, send and
      // clean up the buffered system messages if there is any.
      shell.timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
      shell.timer.initWithCallback(function timerCallback() {
        shell.bufferedSysMsgs.forEach(function sendSysMsg(msg) {
          shell.sendSystemMessage(msg);
        });
        shell.bufferedSysMsgs.length = 0;
        shell.needBufferSysMsgs = false;
        shell.timer = null;
      }, 10000, Ci.nsITimer.TYPE_ONE_SHOT);
    }).bind(this), false);
  },

  handleEvent: function custevt_handleEvent(evt) {
    let detail = evt.detail;
    dump('XXX FIXME : Got a mozContentEvent: ' + detail.type + "\n");

    switch(detail.type) {
      case 'desktop-notification-show':
      case 'desktop-notification-click':
      case 'desktop-notification-close':
        AlertsHelper.handleEvent(detail);
        break;
      case 'webapps-install-granted':
      case 'webapps-install-denied':
        WebappsHelper.handleEvent(detail);
        break;
      case 'select-choicechange':
        FormsHelper.handleEvent(detail);
        break;
      case 'system-message-listener-ready':
        Services.obs.notifyObservers(null, 'system-message-listener-ready', null);
        break;
      case 'remote-debugger-prompt':
        RemoteDebugger.handleEvent(detail);
        break;
      case 'captive-portal-login-cancel':
        CaptivePortalLoginHelper.handleEvent(detail);
        break;
    }
  }
}

var AlertsHelper = {
  _listeners: {},
  _count: 0,

  handleEvent: function alert_handleEvent(detail) {
    if (!detail || !detail.id)
      return;

    let uid = detail.id;
    let listener = this._listeners[uid];
    if (!listener)
     return;

    let topic;
    if (detail.type == "desktop-notification-click") {
      topic = "alertclickcallback";
    } else if (detail.type == "desktop-notification-show") {
      topic = "alertshow";
    } else {
      /* desktop-notification-close */
      topic = "alertfinished";
    }

    if (uid.startsWith("app-notif")) {
      try {
        listener.mm.sendAsyncMessage("app-notification-return", {
          uid: uid,
          topic: topic,
          target: listener.target
        });
      } catch(e) {
        // we get an exception if the app is not launched yet

        gSystemMessenger.sendMessage("notification", {
            clicked: (detail.type === "desktop-notification-click"),
            title: listener.title,
            body: listener.text,
            imageURL: listener.imageURL
          },
          Services.io.newURI(listener.target, null, null),
          Services.io.newURI(listener.manifestURL, null, null)
        );
      }
    } else if (uid.startsWith("alert")) {
      try {
        listener.observer.observe(null, topic, listener.cookie);
      } catch (e) { }
    }

    // we're done with this notification
    if (topic === "alertfinished") {
      delete this._listeners[uid];
    }
  },

  registerListener: function alert_registerListener(alertId, cookie, alertListener) {
    this._listeners[alertId] = { observer: alertListener, cookie: cookie };
  },

  registerAppListener: function alert_registerAppListener(uid, listener) {
    this._listeners[uid] = listener;

    let app = DOMApplicationRegistry.getAppByManifestURL(listener.manifestURL);
    DOMApplicationRegistry.getManifestFor(app.origin, function(manifest) {
      let helper = new ManifestHelper(manifest, app.origin);
      let getNotificationURLFor = function(messages) {
        if (!messages)
          return null;

        for (let i = 0; i < messages.length; i++) {
          let message = messages[i];
          if (message === "notification") {
            return helper.fullLaunchPath();
          } else if (typeof message == "object" && "notification" in message) {
            return helper.resolveFromOrigin(message["notification"]);
          }
        }
      }

      listener.target = getNotificationURLFor(manifest.messages);

      // Bug 816944 - Support notification messages for entry_points.
    });
  },

  showNotification: function alert_showNotification(imageUrl,
                                                    title,
                                                    text,
                                                    textClickable,
                                                    cookie,
                                                    uid,
                                                    bidi,
                                                    lang,
                                                    manifestUrl) {
    function send(appName, appIcon) {
      shell.sendChromeEvent({
        type: "desktop-notification",
        id: uid,
        icon: imageUrl,
        title: title,
        text: text,
        bidi: bidi,
        lang: lang,
        appName: appName,
        appIcon: appIcon,
        manifestURL: manifestUrl
      });
    }

    if (!manifestUrl || !manifestUrl.length) {
      send(null, null);
    }

    // If we have a manifest URL, get the icon and title from the manifest
    // to prevent spoofing.
    let app = DOMApplicationRegistry.getAppByManifestURL(manifestUrl);
    DOMApplicationRegistry.getManifestFor(app.origin, function(aManifest) {
      let helper = new ManifestHelper(aManifest, app.origin);
      send(helper.name, helper.iconURLForSize(128));
    });
  },

  showAlertNotification: function alert_showAlertNotification(imageUrl,
                                                              title,
                                                              text,
                                                              textClickable,
                                                              cookie,
                                                              alertListener,
                                                              name,
                                                              bidi,
                                                              lang) {
    let currentListener = this._listeners[name];
    if (currentListener) {
      currentListener.observer.observe(null, "alertfinished", currentListener.cookie);
    }

    this.registerListener(name, cookie, alertListener);
    this.showNotification(imageUrl, title, text, textClickable, cookie,
                          name, bidi, lang, null);
  },

  closeAlert: function alert_closeAlert(name) {
    shell.sendChromeEvent({
      type: "desktop-notification-close",
      id: name
    });
  },

  receiveMessage: function alert_receiveMessage(aMessage) {
    if (!aMessage.target.assertAppHasPermission("desktop-notification")) {
      Cu.reportError("Desktop-notification message " + aMessage.name +
                     " from a content process with no desktop-notification privileges.");
      return null;
    }

    let data = aMessage.data;
    let listener = {
      mm: aMessage.target,
      title: data.title,
      text: data.text,
      manifestURL: data.manifestURL,
      imageURL: data.imageURL
    }
    this.registerAppListener(data.uid, listener);

    this.showNotification(data.imageURL, data.title, data.text,
                          data.textClickable, null,
                          data.uid, null, null, data.manifestURL);
  },
}

var WebappsHelper = {
  _installers: {},
  _count: 0,

  init: function webapps_init() {
    Services.obs.addObserver(this, "webapps-launch", false);
    Services.obs.addObserver(this, "webapps-ask-install", false);
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
    }
  },

  observe: function webapps_observe(subject, topic, data) {
    let json = JSON.parse(data);
    json.mm = subject;

    switch(topic) {
      case "webapps-launch":
        DOMApplicationRegistry.getManifestFor(json.origin, function(aManifest) {
          if (!aManifest)
            return;

          let manifest = new ManifestHelper(aManifest, json.origin);
          shell.sendChromeEvent({
            "type": "webapps-launch",
            "timestamp": json.timestamp,
            "url": manifest.fullLaunchPath(json.startPoint),
            "manifestURL": json.manifestURL
          });
        });
        break;
      case "webapps-ask-install":
        let id = this.registerInstaller(json);
        shell.sendChromeEvent({
          type: "webapps-ask-install",
          id: id,
          app: json.app
        });
        break;
      case "webapps-close":
        shell.sendChromeEvent({
          "type": "webapps-close",
          "manifestURL": json.manifestURL
        });
        break;
    }
  }
}

let IndexedDBPromptHelper = {
  _quotaPrompt: "indexedDB-quota-prompt",
  _quotaResponse: "indexedDB-quota-response",

  init:
  function IndexedDBPromptHelper_init() {
    Services.obs.addObserver(this, this._quotaPrompt, false);
  },

  uninit:
  function IndexedDBPromptHelper_uninit() {
    Services.obs.removeObserver(this, this._quotaPrompt);
  },

  observe:
  function IndexedDBPromptHelper_observe(subject, topic, data) {
    if (topic != this._quotaPrompt) {
      throw new Error("Unexpected topic!");
    }

    let observer = subject.QueryInterface(Ci.nsIInterfaceRequestor)
                          .getInterface(Ci.nsIObserver);
    let responseTopic = this._quotaResponse;

    setTimeout(function() {
      observer.observe(null, responseTopic,
                       Ci.nsIPermissionManager.DENY_ACTION);
    }, 0);
  }
}

let RemoteDebugger = {
  _promptDone: false,
  _promptAnswer: false,

  prompt: function debugger_prompt() {
    this._promptDone = false;

    shell.sendChromeEvent({
      "type": "remote-debugger-prompt"
    });

    while(!this._promptDone) {
      Services.tm.currentThread.processNextEvent(true);
    }

    return this._promptAnswer;
  },

  handleEvent: function debugger_handleEvent(detail) {
    this._promptAnswer = detail.value;
    this._promptDone = true;
  },

  // Start the debugger server.
  start: function debugger_start() {
    if (!DebuggerServer.initialized) {
      // Ask for remote connections.
      DebuggerServer.init(this.prompt.bind(this));
      DebuggerServer.addActors("chrome://global/content/devtools/dbg-browser-actors.js");
#ifndef MOZ_WIDGET_GONK
      DebuggerServer.addActors("chrome://global/content/devtools/dbg-webconsole-actors.js");
      DebuggerServer.addActors("chrome://global/content/devtools/dbg-gcli-actors.js");
#endif
      if ("nsIProfiler" in Ci) {
        DebuggerServer.addActors("chrome://global/content/devtools/dbg-profiler-actors.js");
      }
      DebuggerServer.addActors("chrome://global/content/devtools/dbg-styleeditor-actors.js");
      DebuggerServer.addActors('chrome://browser/content/dbg-browser-actors.js');
      DebuggerServer.addActors('chrome://browser/content/dbg-webapps-actors.js');
    }

    let port = Services.prefs.getIntPref('devtools.debugger.remote-port') || 6000;
    try {
      DebuggerServer.openListener(port);
    } catch (e) {
      dump('Unable to start debugger server: ' + e + '\n');
    }
  },

  stop: function debugger_stop() {
    if (!DebuggerServer.initialized) {
      return;
    }

    try {
      DebuggerServer.closeListener();
    } catch (e) {
      dump('Unable to stop debugger server: ' + e + '\n');
    }
  }
}

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
      var canvas = document.createElementNS('http://www.w3.org/1999/xhtml',
                                            'canvas');
      var width = window.innerWidth;
      var height = window.innerHeight;
      canvas.setAttribute('width', width);
      canvas.setAttribute('height', height);

      var context = canvas.getContext('2d');
      var flags =
        context.DRAWWINDOW_DRAW_CARET |
        context.DRAWWINDOW_DRAW_VIEW |
        context.DRAWWINDOW_USE_WIDGET_LAYERS;
      context.drawWindow(window, 0, 0, width, height,
                         'rgb(255,255,255)', flags);

      // I can't use sendChromeEvent() here because it doesn't wrap
      // the blob in the detail object correctly. So I use __exposedProps__
      // instead to safely send the chrome detail object to content.
      shell.sendEvent(getContentWindow(), 'mozChromeEvent', {
        __exposedProps__: { type: 'r', file: 'r' },
        type: 'take-screenshot-success',
        file: canvas.mozGetAsFile('screenshot', 'image/png')
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
  let updatePrompt = Cc["@mozilla.org/updates/update-prompt;1"]
                       .createInstance(Ci.nsIUpdatePrompt);
  if (!updatePrompt) {
    return;
  }

  updatePrompt.wrappedJSObject.handleContentStart(shell);
});

(function geolocationStatusTracker() {
  let gGeolocationActive = false;

  Services.obs.addObserver(function(aSubject, aTopic, aData) {
    let oldState = gGeolocationActive;
    if (aData == "starting") {
      gGeolocationActive = true;
    } else if (aData == "shutdown") {
      gGeolocationActive = false;
    }

    if (gGeolocationActive != oldState) {
      shell.sendChromeEvent({
        type: 'geolocation-status',
        active: gGeolocationActive
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
  let gRecordingActiveCount = 0;

  Services.obs.addObserver(function(aSubject, aTopic, aData) {
    let oldCount = gRecordingActiveCount;
    if (aData == "starting") {
      gRecordingActiveCount += 1;
    } else if (aData == "shutdown") {
      gRecordingActiveCount -= 1;
    }

    // We need to track changes from 1 <-> 0
    if (gRecordingActiveCount + oldCount == 1) {
      shell.sendChromeEvent({
        type: 'recording-status',
        active: (gRecordingActiveCount == 1)
      });
    }
}, "recording-device-events", false);
})();

(function volumeStateTracker() {
  Services.obs.addObserver(function(aSubject, aTopic, aData) {
    shell.sendChromeEvent({
      type: 'volume-state-changed',
      active: (aData == 'Shared')
    });
}, 'volume-state-changed', false);
})();

Services.obs.addObserver(function(aSubject, aTopic, aData) {
  let data = JSON.parse(aData);
  shell.sendChromeEvent({
    type: "activity-done",
    success: data.success,
    manifestURL: data.manifestURL,
    pageURL: data.pageURL
  });
}, "activity-done", false);

#ifdef MOZ_WIDGET_GONK
// Devices don't have all the same partition size for /cache where we
// store the http cache.
(function setHTTPCacheSize() {
  let path = Services.prefs.getCharPref("browser.cache.disk.parent_directory");
  let volumeService = Cc["@mozilla.org/telephony/volume-service;1"]
                        .getService(Ci.nsIVolumeService);

  let stats = volumeService.createOrGetVolumeByPath(path).getStats();

  // We must set the size in KB, and keep a bit of free space.
  let size = Math.floor(stats.totalBytes / 1024) - 1024;
  Services.prefs.setIntPref("browser.cache.disk.capacity", size);
}) ()
#endif
