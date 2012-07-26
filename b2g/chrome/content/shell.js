/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- /
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import('resource://gre/modules/XPCOMUtils.jsm');
Cu.import('resource://gre/modules/Services.jsm');
Cu.import('resource://gre/modules/ContactService.jsm');
Cu.import('resource://gre/modules/SettingsChangeNotifier.jsm');
Cu.import('resource://gre/modules/Webapps.jsm');
Cu.import('resource://gre/modules/AlarmService.jsm');
Cu.import('resource://gre/modules/ActivitiesService.jsm');

XPCOMUtils.defineLazyServiceGetter(Services, 'env',
                                   '@mozilla.org/process/environment;1',
                                   'nsIEnvironment');

XPCOMUtils.defineLazyServiceGetter(Services, 'ss',
                                   '@mozilla.org/content/style-sheet-service;1',
                                   'nsIStyleSheetService');

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

function getContentWindow() {
  return shell.contentBrowser.contentWindow;
}

// FIXME Bug 707625
// until we have a proper security model, add some rights to
// the pre-installed web applications
// XXX never grant 'content-camera' to non-gaia apps
function addPermissions(urls) {
  let permissions = [
    'indexedDB-unlimited', 'webapps-manage', 'offline-app', 'pin-app',
    'websettings-read', 'websettings-readwrite',
    'content-camera', 'webcontacts-manage', 'wifi-manage', 'desktop-notification',
    'geolocation', 'device-storage', 'alarms'
  ];

  urls.forEach(function(url) {
    url = url.trim();
    if (url) {
      let uri = Services.io.newURI(url, null, null);
      let allow = Ci.nsIPermissionManager.ALLOW_ACTION;

      permissions.forEach(function(permission) {
        Services.perms.add(uri, permission, allow);
      });
    }
  });
}

var shell = {
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

  start: function shell_start() {
    let homeURL = this.homeURL;
    if (!homeURL) {
      let msg = 'Fatal error during startup: No homescreen found: try setting B2G_HOMESCREEN';
      alert(msg);
      return;
    }

    let manifestURL = this.manifestURL;
    // <html:iframe id="homescreen"
    //              mozbrowser="true" mozallowfullscreen="true"
    //              style="overflow: hidden; -moz-box-flex: 1; border: none;"
    //              src="data:text/html;charset=utf-8,%3C!DOCTYPE html>%3Cbody style='background:black;'>"/>
    let browserFrame =
      document.createElementNS('http://www.w3.org/1999/xhtml', 'html:iframe');
    browserFrame.setAttribute('id', 'homescreen');
    browserFrame.setAttribute('mozbrowser', 'true');
    browserFrame.setAttribute('mozapp', manifestURL);
    browserFrame.setAttribute('mozallowfullscreen', 'true');
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

    // Until the volume can be set from the content side, set it to a
    // a specific value when the device starts. This way the front-end
    // can display a notification when the volume change and show a volume
    // level modified from this point.
    // try catch block must be used since the emulator fails here. bug 746429
    try {
      Services.audioManager.masterVolume = 0.5;
    } catch(e) {
      dump('Error setting master volume: ' + e + '\n');
    }

    let domains = "";
    try {
      domains = Services.prefs.getCharPref('b2g.privileged.domains');
    } catch(e) {}

    addPermissions(domains.split(","));

    CustomEventManager.init();
    WebappsHelper.init();

    // XXX could factor out into a settings->pref map.  Not worth it yet.
    SettingsListener.observe("debug.fps.enabled", false, function(value) {
      Services.prefs.setBoolPref("layers.acceleration.draw-fps", value);
    });
    SettingsListener.observe("debug.paint-flashing.enabled", false, function(value) {
      Services.prefs.setBoolPref("nglayout.debug.paint_flashing", value);
    });

    this.contentBrowser.src = homeURL;
  },

  stop: function shell_stop() {
    window.removeEventListener('keydown', this, true);
    window.removeEventListener('keypress', this, true);
    window.removeEventListener('keyup', this, true);
    window.removeEventListener('MozApplicationManifest', this);
    window.removeEventListener('mozfullscreenchange', this);
    window.removeEventListener('sizemodechange', this);
    this.contentBrowser.removeEventListener('mozbrowserloadstart', this, true);

#ifndef MOZ_WIDGET_GONK
    delete Services.audioManager;
#endif
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
        if (window.windowState == window.STATE_MINIMIZED) {
          this.contentBrowser.setVisible(false);
        } else {
          this.contentBrowser.setVisible(true);
        }
        break;
      case 'mozbrowserloadstart':
        if (content.document.location == 'about:blank')
          return;

        this.contentBrowser.removeEventListener('mozbrowserloadstart', this, true);

        let chromeWindow = window.QueryInterface(Ci.nsIDOMChromeWindow);
        chromeWindow.browserDOMWindow = new nsBrowserAccess();

        this.sendEvent(window, 'ContentStart');
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
    this.sendEvent(getContentWindow(), "mozChromeEvent", details);
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
  let origin = Services.io.newURI(msg.manifest, null, null).prePath;
  shell.sendChromeEvent({
    type: 'open-app',
    url: msg.uri,
    origin: origin,
    manifest: msg.manifest
  });
}, 'system-messages-open-app', false);

(function Repl() {
  if (!Services.prefs.getBoolPref('b2g.remote-js.enabled')) {
    return;
  }
  const prompt = 'JS> ';
  let output;
  let reader = {
    onInputStreamReady : function repl_readInput(input) {
      let sin = Cc['@mozilla.org/scriptableinputstream;1']
                  .createInstance(Ci.nsIScriptableInputStream);
      sin.init(input);
      try {
        let val = eval(sin.read(sin.available()));
        let ret = (typeof val === 'undefined') ? 'undefined\n' : val + '\n';
        output.write(ret, ret.length);
        // TODO: check if socket has been closed
      } catch (e) {
        if (e.result === Cr.NS_BASE_STREAM_CLOSED ||
            (typeof e === 'object' && e.result === Cr.NS_BASE_STREAM_CLOSED)) {
          return;
        }
        let message = (typeof e === 'object') ? e.message + '\n' : e + '\n';
        output.write(message, message.length);
      }
      output.write(prompt, prompt.length);
      input.asyncWait(reader, 0, 0, Services.tm.mainThread);
    }
  }
  let listener = {
    onSocketAccepted: function repl_acceptConnection(serverSocket, clientSocket) {
      dump('Accepted connection on ' + clientSocket.host + '\n');
      let input = clientSocket.openInputStream(Ci.nsITransport.OPEN_BLOCKING, 0, 0)
                              .QueryInterface(Ci.nsIAsyncInputStream);
      output = clientSocket.openOutputStream(Ci.nsITransport.OPEN_BLOCKING, 0, 0);
      output.write(prompt, prompt.length);
      input.asyncWait(reader, 0, 0, Services.tm.mainThread);
    }
  }
  let serverPort = Services.prefs.getIntPref('b2g.remote-js.port');
  let serverSocket = Cc['@mozilla.org/network/server-socket;1']
                       .createInstance(Ci.nsIServerSocket);
  serverSocket.init(serverPort, true, -1);
  dump('Opened socket on ' + serverSocket.port + '\n');
  serverSocket.asyncListen(listener);
})();

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
    }
  }
}

var AlertsHelper = {
  _listeners: {},
  _count: 0,

  handleEvent: function alert_handleEvent(detail) {
    if (!detail || !detail.id)
      return;

    let listener = this._listeners[detail.id];
    let topic = detail.type == "desktop-notification-click" ? "alertclickcallback" : "alertfinished";
    listener.observer.observe(null, topic, listener.cookie);

    // we're done with this notification
    if (topic === "alertfinished")
      delete this._listeners[detail.id];
  },

  registerListener: function alert_registerListener(cookie, alertListener) {
    let id = "alert" + this._count++;
    this._listeners[id] = { observer: alertListener, cookie: cookie };
    return id;
  },

  showAlertNotification: function alert_showAlertNotification(imageUrl,
                                                              title,
                                                              text,
                                                              textClickable,
                                                              cookie,
                                                              alertListener,
                                                              name)
  {
    let id = this.registerListener(cookie, alertListener);
    shell.sendChromeEvent({
      type: "desktop-notification",
      id: id,
      icon: imageUrl,
      title: title,
      text: text
    });
  }
}

var WebappsHelper = {
  _installers: {},
  _count: 0,

  init: function webapps_init() {
    Services.obs.addObserver(this, "webapps-launch", false);
    Services.obs.addObserver(this, "webapps-ask-install", false);
    DOMApplicationRegistry.allAppsLaunchable = true;
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
    switch(topic) {
      case "webapps-launch":
        DOMApplicationRegistry.getManifestFor(json.origin, function(aManifest) {
          if (!aManifest)
            return;

          let manifest = new DOMApplicationManifest(aManifest, json.origin);
          shell.sendChromeEvent({
            "type": "webapps-launch",
            "url": manifest.fullLaunchPath(json.startPoint),
            "origin": json.origin
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
    }
  }
}

// Start the debugger server.
function startDebugger() {
  if (!DebuggerServer.initialized) {
    // Allow remote connections.
    DebuggerServer.init(function () { return true; });
    DebuggerServer.addBrowserActors();
    DebuggerServer.addActors('chrome://browser/content/dbg-browser-actors.js');
  }

  let port = Services.prefs.getIntPref('devtools.debugger.remote-port') || 6000;
  try {
    DebuggerServer.openListener(port);
  } catch (e) {
    dump('Unable to start debugger server: ' + e + '\n');
  }
}

window.addEventListener('ContentStart', function(evt) {
  if (Services.prefs.getBoolPref('devtools.debugger.remote-enabled')) {
    startDebugger();
  }
});

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

      shell.sendChromeEvent({
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

Services.obs.addObserver(function ContentHandler(subject, topic, data) {
  let handler = JSON.parse(data);
  new MozActivity({
    name: 'view',
    data: {
      type: handler.type,
      url: handler.url
    }
  });
}, 'content-handler', false);

(function geolocationStatusTracker() {
  let gGeolocationActiveCount = 0;

  Services.obs.addObserver(function(aSubject, aTopic, aData) {
    let oldCount = gGeolocationActiveCount;
    if (aData == "starting") {
      gGeolocationActiveCount += 1;
    } else if (aData == "shutdown") {
      gGeolocationActiveCount -= 1;
    }

    // We need to track changes from 1 <-> 0
    if (gGeolocationActiveCount + oldCount == 1) {
      shell.sendChromeEvent({
        type: 'geolocation-status',
        active: (gGeolocationActiveCount == 1)
      });
    }
}, "geolocation-device-events", false);
})();
