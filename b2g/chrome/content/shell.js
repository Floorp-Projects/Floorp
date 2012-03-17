/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- /
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const CC = Components.Constructor;
const Cr = Components.results;

const LocalFile = CC('@mozilla.org/file/local;1',
                     'nsILocalFile',
                     'initWithPath');

Cu.import('resource://gre/modules/XPCOMUtils.jsm');
Cu.import('resource://gre/modules/Services.jsm');
Cu.import('resource://gre/modules/ContactService.jsm');

XPCOMUtils.defineLazyGetter(Services, 'env', function() {
  return Cc['@mozilla.org/process/environment;1']
           .getService(Ci.nsIEnvironment);
});

XPCOMUtils.defineLazyGetter(Services, 'ss', function() {
  return Cc['@mozilla.org/content/style-sheet-service;1']
           .getService(Ci.nsIStyleSheetService);
});

XPCOMUtils.defineLazyGetter(Services, 'idle', function() {
  return Cc['@mozilla.org/widget/idleservice;1']
           .getService(Ci.nsIIdleService);
});

XPCOMUtils.defineLazyGetter(Services, 'audioManager', function() {
  return Cc['@mozilla.org/telephony/audiomanager;1']
           .getService(Ci.nsIAudioManager);
});

XPCOMUtils.defineLazyServiceGetter(Services, 'fm', function() {
  return Cc['@mozilla.org/focus-manager;1']
           .getService(Ci.nsFocusManager);
});

#ifndef MOZ_WIDGET_GONK
// In order to use http:// scheme instead of file:// scheme
// (that is much more restricted) the following code kick-off
// a local http server listening on http://127.0.0.1:7777 and
// http://localhost:7777.
function startupHttpd(baseDir, port) {
  const httpdURL = 'chrome://browser/content/httpd.js';
  let httpd = {};
  Services.scriptloader.loadSubScript(httpdURL, httpd);
  let server = new httpd.nsHttpServer();
  server.registerDirectory('/', new LocalFile(baseDir));
  server.registerContentType('appcache', 'text/cache-manifest');
  server.start(port);
}
#endif

// FIXME Bug 707625
// until we have a proper security model, add some rights to
// the pre-installed web applications
// XXX never grant 'content-camera' to non-gaia apps
function addPermissions(urls) {
  let permissions = [
    'indexedDB', 'indexedDB-unlimited', 'webapps-manage', 'offline-app',
    'content-camera', 'webcontacts-manage', 'wifi-manage'
  ];
  urls.forEach(function(url) {
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

    let urls = Services.prefs.getCharPref('browser.homescreenURL').split(',');
    for (let i = 0; i < urls.length; i++) {
      let url = urls[i];
      if (url.substring(0, 7) != 'file://')
        return url;

      let file = new LocalFile(url.substring(7, url.length));
      if (file.exists())
        return url;
    }
    return null;
  },

  start: function shell_init() {
    let homeURL = this.homeURL;
    if (!homeURL) {
      let msg = 'Fatal error during startup: [No homescreen found]';
      return alert(msg);
    }

    ['keydown', 'keypress', 'keyup'].forEach((function listenKey(type) {
      window.addEventListener(type, this, false, true);
      window.addEventListener(type, this, true, true);
    }).bind(this));

    window.addEventListener('MozApplicationManifest', this);
    window.addEventListener('mozfullscreenchange', this);
    this.contentBrowser.addEventListener('load', this, true);

    // Until the volume can be set from the content side, set it to a
    // a specific value when the device starts. This way the front-end
    // can display a notification when the volume change and show a volume
    // level modified from this point.
    try {
      Services.audioManager.masterVolume = 0.5;
    } catch(e) {}

    try {
      Services.io.offline = false;

      let fileScheme = 'file://';
      if (homeURL.substring(0, fileScheme.length) == fileScheme) {
#ifndef MOZ_WIDGET_GONK
        homeURL = homeURL.replace(fileScheme, '');

        let baseDir = homeURL.split('/');
        baseDir.pop();
        baseDir = baseDir.join('/');

        const SERVER_PORT = 7777;
        startupHttpd(baseDir, SERVER_PORT);

        let baseHost = 'http://localhost';
        homeURL = homeURL.replace(baseDir, baseHost + ':' + SERVER_PORT);
#else
        homeURL = 'http://localhost:7777' + homeURL.replace(fileScheme, '');
#endif
      }
      addPermissions([homeURL]);
    } catch (e) {
      let msg = 'Fatal error during startup: [' + e + '[' + homeURL + ']';
      return alert(msg);
    }

    // Load webapi.js as a frame script
    let frameScriptUrl = 'chrome://browser/content/webapi.js';
    try {
      messageManager.loadFrameScript(frameScriptUrl, true);
    } catch (e) {
      dump('Error loading ' + frameScriptUrl + ' as a frame script: ' + e + '\n');
    }

    let browser = this.contentBrowser;
    browser.homePage = homeURL;
    browser.goHome();
  },

  stop: function shell_stop() {
    window.removeEventListener('MozApplicationManifest', this);
    window.removeEventListener('mozfullscreenchange', this);
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

    let volume = audioManager.masterVolume + delta / steps;
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

(function PowerManager() {
  let idleHandler = {
    observe: function(subject, topic, time) {
      if (topic === "idle") {
        // TODO: Check wakelock status. See bug 697132.
        screen.mozEnabled = false;
      }
    },
  }
  let idleTimeout = Services.prefs.getIntPref("power.screen.timeout");
  if (idleTimeout) {
    Services.idle.addIdleObserver(idleHandler, idleTimeout);
  }
})();

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

