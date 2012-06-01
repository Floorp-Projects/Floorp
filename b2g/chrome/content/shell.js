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
Cu.import('resource://gre/modules/Webapps.jsm');

XPCOMUtils.defineLazyServiceGetter(Services, 'env',
                                   '@mozilla.org/process/environment;1',
                                   'nsIEnvironment');

XPCOMUtils.defineLazyServiceGetter(Services, 'ss',
                                   '@mozilla.org/content/style-sheet-service;1',
                                   'nsIStyleSheetService');

XPCOMUtils.defineLazyServiceGetter(Services, 'idle',
                                   '@mozilla.org/widget/idleservice;1',
                                   'nsIIdleService');

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

// FIXME Bug 707625
// until we have a proper security model, add some rights to
// the pre-installed web applications
// XXX never grant 'content-camera' to non-gaia apps
function addPermissions(urls) {
  let permissions = [
    'indexedDB', 'indexedDB-unlimited', 'webapps-manage', 'offline-app', 'pin-app',
    'websettings-read', 'websettings-readwrite',
    'content-camera', 'webcontacts-manage', 'wifi-manage', 'desktop-notification',
    'geolocation', 'device-storage'
  ];
  urls.forEach(function(url) {
    url = url.trim();
    let uri = Services.io.newURI(url, null, null);
    let allow = Ci.nsIPermissionManager.ALLOW_ACTION;

    permissions.forEach(function(permission) {
      Services.perms.add(uri, permission, allow);
    });
  });
}

var shell = {
  isDebug: false,

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

  start: function shell_start() {
    let homeURL = this.homeURL;
    if (!homeURL) {
      let msg = 'Fatal error during startup: No homescreen found: try setting B2G_HOMESCREEN';
      alert(msg);
      return;
    }

    ['keydown', 'keypress', 'keyup'].forEach((function listenKey(type) {
      window.addEventListener(type, this, false, true);
      window.addEventListener(type, this, true, true);
    }).bind(this));

    window.addEventListener('MozApplicationManifest', this);
    window.addEventListener('mozfullscreenchange', this);
    window.addEventListener('sizemodechange', this);
    this.contentBrowser.addEventListener('load', this, true);

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

    // Load webapi.js as a frame script
    let frameScriptUrl = 'chrome://browser/content/webapi.js';
    try {
      messageManager.loadFrameScript(frameScriptUrl, true);
    } catch (e) {
      dump('Error loading ' + frameScriptUrl + ' as a frame script: ' + e + '\n');
    }

    CustomEventManager.init();

    WebappsHelper.init();

    let browser = this.contentBrowser;
    browser.homePage = homeURL;
    browser.goHome();
  },

  stop: function shell_stop() {
    ['keydown', 'keypress', 'keyup'].forEach((function unlistenKey(type) {
      window.removeEventListener(type, this, false, true);
      window.removeEventListener(type, this, true, true);
    }).bind(this));

    window.addEventListener('MozApplicationManifest', this);
    window.removeEventListener('MozApplicationManifest', this);
    window.removeEventListener('mozfullscreenchange', this);
    window.removeEventListener('sizemodechange', this);
    this.contentBrowser.removeEventListener('load', this, true);

#ifndef MOZ_WIDGET_GONK
    delete Services.audioManager;
#endif
  },

  toggleDebug: function shell_toggleDebug() {
    this.isDebug = !this.isDebug;

    if (this.isDebug) {
      Services.prefs.setBoolPref("layers.acceleration.draw-fps", true);
      Services.prefs.setBoolPref("nglayout.debug.paint_flashing", true);
    } else {
      Services.prefs.setBoolPref("layers.acceleration.draw-fps", false);
      Services.prefs.setBoolPref("nglayout.debug.paint_flashing", false);
    }
  },
 
  changeVolume: function shell_changeVolume(delta) {
    let steps = 10;
    try {
      steps = Services.prefs.getIntPref("media.volume.steps");
      if (steps <= 0)
        steps = 1;
    } catch(e) {}

    let audioManager = Services.audioManager;
    if (!audioManager)
      return;

    let currentVolume = audioManager.masterVolume;
    let newStep = Math.round(steps * Math.sqrt(currentVolume)) + delta;
    let volume = (newStep / steps) * (newStep / steps);

    if (volume > 1)
      volume = 1;
    if (volume < 0)
      volume = 0;
    audioManager.masterVolume = volume;
  },

  forwardKeyToHomescreen: function shell_forwardKeyToHomescreen(evt) {
    let generatedEvent = content.document.createEvent('KeyboardEvent');
    generatedEvent.initKeyEvent(evt.type, true, true, evt.view, evt.ctrlKey,
                                evt.altKey, evt.shiftKey, evt.metaKey,
                                evt.keyCode, evt.charCode);

    content.dispatchEvent(generatedEvent);
  },

  handleEvent: function shell_handleEvent(evt) {
    switch (evt.type) {
      case 'keydown':
      case 'keyup':
      case 'keypress':
        // If the home key is pressed, always forward it to the homescreen
        if (evt.eventPhase == evt.CAPTURING_PHASE) {
          if (evt.keyCode == evt.VK_DOM_HOME) {
            window.setTimeout(this.forwardKeyToHomescreen, 0, evt);
            evt.preventDefault();
            evt.stopPropagation();
          } 
          return;
        }

        // If one of the other keys is used in an application and is
        // cancelled via preventDefault, do nothing.
        let homescreen = (evt.target.ownerDocument.defaultView == content);
        if (!homescreen && evt.defaultPrevented)
          return;

        // If one of the other keys is used in an application and is
        // not used forward it to the homescreen
        if (!homescreen)
          window.setTimeout(this.forwardKeyToHomescreen, 0, evt);

        // For debug purposes and because some of the APIs are not yet exposed
        // to the content, let's react on some of the keyup events.
        if (evt.type == 'keyup') {
          switch (evt.keyCode) {
            case evt.DOM_VK_F5:
              if (Services.prefs.getBoolPref('b2g.keys.search.enabled'))
                this.toggleDebug();
              break;
  
            case evt.DOM_VK_PAGE_DOWN:
              this.changeVolume(-1);
              break;
  
            case evt.DOM_VK_PAGE_UP:
              this.changeVolume(1);
              break;
          }
        }
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
          this.contentBrowser.docShell.isActive = false;
        } else {
          this.contentBrowser.docShell.isActive = true;
        }
        break;
      case 'load':
        this.contentBrowser.removeEventListener('load', this, true);

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

          let documentURI = contentWindow.document.documentURIObject;
          if (!Services.perms.testPermission(documentURI, 'offline-app')) {
            if (Services.prefs.getBoolPref('browser.offline-apps.notify')) {
              // FIXME Bug 710729 - Add a UI for offline cache notifications
              return;
            }
            return;
          }

          Services.perms.add(documentURI, 'offline-app',
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
  }
};

function nsBrowserAccess() {
}

nsBrowserAccess.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIBrowserDOMWindow]),

  openURI: function openURI(uri, opener, where, context) {
    // TODO This should be replaced by an 'open-browser-window' intent
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

// Pipe `console` log messages to the nsIConsoleService which writes them
// to logcat.
Services.obs.addObserver(function onConsoleAPILogEvent(subject, topic, data) {
  let message = subject.wrappedJSObject;
  let prefix = "Content JS " + message.level.toUpperCase() +
               " at " + message.filename + ":" + message.lineNumber +
               " in " + (message.functionName || "anonymous") + ": ";
  Services.console.logStringMessage(prefix + Array.join(message.arguments, " "));
}, "console-api-log-event", false);

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
      content.addEventListener("mozContentEvent", this, false, true);
    }).bind(this), false);
  },

  handleEvent: function custevt_handleEvent(evt) {
    let detail = evt.detail;
    dump("XXX FIXME : Got a mozContentEvent: " + detail.type);

    switch(detail.type) {
      case "desktop-notification-click":
      case "desktop-notification-close":
        AlertsHelper.handleEvent(detail);
        break;
      case "webapps-install-granted":
      case "webapps-install-denied":
        WebappsHelper.handleEvent(detail);
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

  showAlertNotification: function alert_showAlertNotification(imageUrl, title, text, textClickable, 
                                                              cookie, alertListener, name) {
    let id = this.registerListener(cookie, alertListener);
    shell.sendEvent(content, "mozChromeEvent", { type: "desktop-notification", id: id, icon: imageUrl, 
                                                 title: title, text: text } );
  }
}

var WebappsHelper = {
  _installers: {},
  _count: 0,

  init: function webapps_init() {
    Services.obs.addObserver(this, "webapps-launch", false);
    Services.obs.addObserver(this, "webapps-ask-install", false);
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
          shell.sendEvent(content, "mozChromeEvent", {
            "type": "webapps-launch",
            "url": manifest.fullLaunchPath(json.startPoint),
            "origin": json.origin
          });
        });
        break;
      case "webapps-ask-install":
        let id = this.registerInstaller(json);
        shell.sendEvent(content, "mozChromeEvent", { type: "webapps-ask-install", id: id, app: json.app } );
        break;
    }
  }
}

// Start the debugger server.
function startDebugger() {
  if (!DebuggerServer.initialized) {
    DebuggerServer.init();
    DebuggerServer.addActors('chrome://browser/content/dbg-browser-actors.js');
  }

  let port = Services.prefs.getIntPref('devtools.debugger.port') || 6000;
  try {
    DebuggerServer.openListener(port, false);
  } catch (e) {
    dump('Unable to start debugger server: ' + e + '\n');
  }
}

window.addEventListener('ContentStart', function(evt) {
  if (Services.prefs.getBoolPref('devtools.debugger.enabled')) {
    startDebugger();
  }
});


// Once Bug 731746 - Allow chrome JS object to implement nsIDOMEventTarget
// is resolved this helper could be removed.
var SettingsListener = {
  _callbacks: {},

  init: function sl_init() {
    if ('mozSettings' in navigator && navigator.mozSettings)
      navigator.mozSettings.onsettingchange = this.onchange.bind(this);
  },

  onchange: function sl_onchange(evt) {
    var callback = this._callbacks[evt.settingName];
    if (callback) {
      callback(evt.settingValue);
    }
  },

  observe: function sl_observe(name, defaultValue, callback) {
    var settings = window.navigator.mozSettings;
    if (!settings) {
      window.setTimeout(function() { callback(defaultValue); });
      return;
    }

    if (!callback || typeof callback !== 'function') {
      throw new Error('Callback is not a function');
    }

    var req = settings.getLock().get(name);
    req.addEventListener('success', (function onsuccess() {
      callback(typeof(req.result[name]) != 'undefined' ?
        req.result[name] : defaultValue);
    }));

    this._callbacks[name] = callback;
  }
};

SettingsListener.init();

SettingsListener.observe('language.current', 'en-US', function(value) {
  Services.prefs.setCharPref('intl.accept_languages', value);
});


(function PowerManager() {
  // This will eventually be moved to content, so use content API as
  // much as possible here. TODO: Bug 738530
  let power = navigator.mozPower;
  let idleHandler = function idleHandler(subject, topic, time) {
    if (topic !== 'idle')
      return;

    if (power.getWakeLockState("screen") != "locked-foreground") {
      navigator.mozPower.screenEnabled = false;
    }
  }

  let wakeLockHandler = function(topic, state) {
    // Turn off the screen when no one needs the it or all of them are
    // invisible, otherwise turn the screen on. Note that the CPU
    // might go to sleep as soon as the screen is turned off and
    // acquiring wake lock will not bring it back (actually the code
    // is not executed at all).
    if (topic === 'screen') {
      if (state != "locked-foreground") {
        if (Services.idle.idleTime > idleTimeout*1000) {
          navigator.mozPower.screenEnabled = false;
        }
      } else {
        navigator.mozPower.screenEnabled = true;
      }
    } else if (topic == 'cpu') {
      navigator.mozPower.cpuSleepAllowed = (state != 'locked-foreground' &&
                                            state != 'locked-background');
    }
  }

  let idleTimeout = Services.prefs.getIntPref('power.screen.timeout');
  if (!('mozSettings' in navigator))
    return;

  let request = navigator.mozSettings.getLock().get('power.screen.timeout');
  request.onsuccess = function onSuccess() {
    idleTimeout = request.result['power.screen.timeout'] || idleTimeout;
    if (!idleTimeout)
      return;

    Services.idle.addIdleObserver(idleHandler, idleTimeout);
    power.addWakeLockListener(wakeLockHandler);
  };

  request.onerror = function onError() {
    if (!idleTimeout)
      return;

    Services.idle.addIdleObserver(idleHandler, idleTimeout);
    power.addWakeLockListener(wakeLockHandler);
  };

  SettingsListener.observe('power.screen.timeout', idleTimeout, function(value) {
    if (!value)
      return;

    Services.idle.removeIdleObserver(idleHandler, idleTimeout);
    idleTimeout = value;
    Services.idle.addIdleObserver(idleHandler, idleTimeout);
  });

  window.addEventListener('unload', function removeIdleObjects() {
    Services.idle.removeIdleObserver(idleHandler, idleTimeout);
    power.removeWakeLockListener(wakeLockHandler);
  });
})();


(function RILSettingsToPrefs() {
  ['ril.data.enabled', 'ril.data.roaming.enabled'].forEach(function(key) {
    SettingsListener.observe(key, false, function(value) {
      Services.prefs.setBoolPref(key, value);
    });
  });

  ['ril.data.apn', 'ril.data.user', 'ril.data.passwd'].forEach(function(key) {
    SettingsListener.observe(key, false, function(value) {
      Services.prefs.setCharPref(key, value);
    });
  });
})();

