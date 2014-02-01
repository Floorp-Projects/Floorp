/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Preload some things, in an attempt to make app startup faster.
//
// This script is run when the preallocated process starts.  It is injected as
// a frame script.

const BrowserElementIsPreloaded = true;

(function (global) {
  "use strict";

  let Cu = Components.utils;
  let Cc = Components.classes;
  let Ci = Components.interfaces;

  Cu.import("resource://gre/modules/AppsServiceChild.jsm");
  Cu.import("resource://gre/modules/AppsUtils.jsm");
  Cu.import("resource://gre/modules/BrowserElementPromptService.jsm");
  Cu.import("resource://gre/modules/CSPUtils.jsm");
  Cu.import("resource://gre/modules/DOMRequestHelper.jsm");
  Cu.import("resource://gre/modules/FileUtils.jsm");
  Cu.import("resource://gre/modules/Geometry.jsm");
  Cu.import("resource://gre/modules/IndexedDBHelper.jsm");
  Cu.import("resource://gre/modules/NetUtil.jsm");
  Cu.import("resource://gre/modules/Services.jsm");
  Cu.import("resource://gre/modules/SettingsDB.jsm");
  Cu.import("resource://gre/modules/SettingsQueue.jsm");
  Cu.import("resource://gre/modules/XPCOMUtils.jsm");

  Cc["@mozilla.org/appshell/appShellService;1"].getService(Ci["nsIAppShellService"]);
  Cc["@mozilla.org/appshell/window-mediator;1"].getService(Ci["nsIWindowMediator"]);
  Cc["@mozilla.org/AppsService;1"].getService(Ci["nsIAppsService"]);
  Cc["@mozilla.org/base/telemetry;1"].getService(Ci["nsITelemetry"]);
  Cc["@mozilla.org/categorymanager;1"].getService(Ci["nsICategoryManager"]);
  Cc["@mozilla.org/childprocessmessagemanager;1"].getService(Ci["nsIMessageSender"]);
  Cc["@mozilla.org/consoleservice;1"].getService(Ci["nsIConsoleService"]);
  Cc["@mozilla.org/cookieService;1"].getService(Ci["nsICookieService"]);
  Cc["@mozilla.org/docshell/urifixup;1"].getService(Ci["nsIURIFixup"]);
  Cc["@mozilla.org/dom/dom-request-service;1"].getService(Ci["nsIDOMRequestService"]);
  Cc["@mozilla.org/embedcomp/prompt-service;1"].getService(Ci["nsIPromptService"]);
  Cc["@mozilla.org/embedcomp/window-watcher;1"].getService(Ci["nsIWindowWatcher"]);
  Cc["@mozilla.org/eventlistenerservice;1"].getService(Ci["nsIEventListenerService"]);
  Cc["@mozilla.org/focus-manager;1"].getService(Ci["nsIFocusManager"]);
  Cc["@mozilla.org/intl/nslocaleservice;1"].getService(Ci["nsILocaleService"]);
  Cc["@mozilla.org/intl/stringbundle;1"].getService(Ci["nsIStringBundleService"]);
  Cc["@mozilla.org/layout/content-policy;1"].getService(Ci["nsIContentPolicy"]);
  Cc["@mozilla.org/message-loop;1"].getService(Ci["nsIMessageLoop"]);
  Cc["@mozilla.org/moz/jssubscript-loader;1"].getService(Ci["mozIJSSubScriptLoader"]);
  Cc["@mozilla.org/network/application-cache-service;1"].getService(Ci["nsIApplicationCacheService"]);
  Cc["@mozilla.org/network/dns-service;1"].getService(Ci["nsIDNSService"]);
  Cc["@mozilla.org/network/effective-tld-service;1"].getService(Ci["nsIEffectiveTLDService"]);
  Cc["@mozilla.org/network/idn-service;1"].getService(Ci["nsIIDNService"]);
  Cc["@mozilla.org/network/io-service;1"].getService(Ci["nsIIOService2"]);
  Cc["@mozilla.org/network/mime-hdrparam;1"].getService(Ci["nsIMIMEHeaderParam"]);
  Cc["@mozilla.org/network/protocol-proxy-service;1"].getService(Ci["nsIProtocolProxyService"]);
  Cc["@mozilla.org/network/socket-transport-service;1"].getService(Ci["nsISocketTransportService"]);
  Cc["@mozilla.org/network/stream-transport-service;1"].getService(Ci["nsIStreamTransportService"]);
  Cc["@mozilla.org/network/url-parser;1?auth=maybe"].getService(Ci["nsIURLParser"]);
  Cc["@mozilla.org/network/url-parser;1?auth=no"].getService(Ci["nsIURLParser"]);
  Cc["@mozilla.org/network/url-parser;1?auth=yes"].getService(Ci["nsIURLParser"]);
  Cc["@mozilla.org/observer-service;1"].getService(Ci["nsIObserverService"]);
  Cc["@mozilla.org/permissionmanager;1"].getService(Ci["nsIPermissionManager"]);
  Cc["@mozilla.org/preferences-service;1"].getService(Ci["nsIPrefBranch"]);
  Cc["@mozilla.org/scriptsecuritymanager;1"].getService(Ci["nsIScriptSecurityManager"]);
  Cc["@mozilla.org/storage/service;1"].getService(Ci["mozIStorageService"]);
  Cc["@mozilla.org/system-info;1"].getService(Ci["nsIPropertyBag2"]);
  Cc["@mozilla.org/thread-manager;1"].getService(Ci["nsIThreadManager"]);
  Cc["@mozilla.org/toolkit/app-startup;1"].getService(Ci["nsIAppStartup"]);
  Cc["@mozilla.org/uriloader;1"].getService(Ci["nsIURILoader"]);
  Cc["@mozilla.org/contentsecuritypolicy;1"].createInstance(Ci["nsIContentSecurityPolicy"]);

  /* Applications Specific Helper */
  try {
    // May throw if we don't have the settings permission
    navigator.mozSettings;
  } catch(e) {
  }

  try {
    if (Services.prefs.getBoolPref("dom.sysmsg.enabled")) {
      Cc["@mozilla.org/system-message-manager;1"].getService(Ci["nsIDOMNavigatorSystemMessages"]);
    }
  } catch(e) {
  }

  try {
    if (Services.prefs.getBoolPref("dom.mozInputMethod.enabled")) {
      Services.scriptloader.loadSubScript("chrome://global/content/forms.js", global);
    }
  } catch (e) {
  }

  Services.scriptloader.loadSubScript("chrome://global/content/BrowserElementPanning.js", global);
  Services.scriptloader.loadSubScript("chrome://global/content/BrowserElementChildPreload.js", global);

  Services.io.getProtocolHandler("app");
  Services.io.getProtocolHandler("default");

  docShell.isActive = false;
  docShell.createAboutBlankContentViewer(null);

})(this);
