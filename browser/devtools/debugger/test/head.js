/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

let tempScope = {};
Cu.import("resource://gre/modules/Services.jsm", tempScope);
Cu.import("resource://gre/modules/devtools/dbg-server.jsm", tempScope);
Cu.import("resource://gre/modules/devtools/dbg-client.jsm", tempScope);
Cu.import("resource:///modules/source-editor.jsm", tempScope);
Cu.import("resource:///modules/devtools/gDevTools.jsm", tempScope);
Cu.import("resource://gre/modules/devtools/Loader.jsm", tempScope);
let Services = tempScope.Services;
let SourceEditor = tempScope.SourceEditor;
let DebuggerServer = tempScope.DebuggerServer;
let DebuggerTransport = tempScope.DebuggerTransport;
let DebuggerClient = tempScope.DebuggerClient;
let gDevTools = tempScope.gDevTools;
let devtools = tempScope.devtools;
let TargetFactory = devtools.TargetFactory;

// Import the GCLI test helper
let testDir = gTestPath.substr(0, gTestPath.lastIndexOf("/"));
Services.scriptloader.loadSubScript(testDir + "/helpers.js", this);

const EXAMPLE_URL = "http://example.com/browser/browser/devtools/debugger/test/";
const TAB1_URL = EXAMPLE_URL + "browser_dbg_tab1.html";
const TAB2_URL = EXAMPLE_URL + "browser_dbg_tab2.html";
const STACK_URL = EXAMPLE_URL + "browser_dbg_stack.html";

// Enable logging and remote debugging for the relevant tests.
let gEnableRemote = Services.prefs.getBoolPref("devtools.debugger.remote-enabled");
let gEnableLogging = Services.prefs.getBoolPref("devtools.debugger.log");
Services.prefs.setBoolPref("devtools.debugger.remote-enabled", true);
Services.prefs.setBoolPref("devtools.debugger.log", true);

registerCleanupFunction(function() {
  Services.prefs.setBoolPref("devtools.debugger.remote-enabled", gEnableRemote);
  Services.prefs.setBoolPref("devtools.debugger.log", gEnableLogging);

  // Properly shut down the server to avoid memory leaks.
  DebuggerServer.destroy();
});

if (!DebuggerServer.initialized) {
  DebuggerServer.init(function() true);
  DebuggerServer.addBrowserActors();
}

waitForExplicitFinish();

function addWindow() {
  let windowReference = window.open();
  let chromeWindow = windowReference
    .QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIWebNavigation)
    .QueryInterface(Ci.nsIDocShellTreeItem).rootTreeItem
    .QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindow);

  return chromeWindow;
}

function addTab(aURL, aOnload, aWindow) {
  let targetWindow = aWindow || window;
  let targetBrowser = targetWindow.gBrowser;

  targetWindow.focus();
  targetBrowser.selectedTab = targetBrowser.addTab(aURL);

  let tab = targetBrowser.selectedTab;
  let win = tab.linkedBrowser.contentWindow;
  let expectedReadyState = aURL == "about:blank" ? ["interactive", "complete"] : ["complete"];

  if (aOnload) {
    let handler = function() {
      if (tab.linkedBrowser.currentURI.spec != aURL ||
          expectedReadyState.indexOf((win.document || {}).readyState) == -1) {
        return;
      }
      tab.removeEventListener("load", handler, false);
      executeSoon(aOnload);
    }
    tab.addEventListener("load", handler, false);
  }

  return tab;
}

function removeTab(aTab, aWindow) {
  let targetWindow = aWindow || window;
  let targetBrowser = targetWindow.gBrowser;

  targetBrowser.removeTab(aTab);
}

function closeDebuggerAndFinish(aRemoteFlag, aCallback, aWindow) {
  let debuggerClosed = false;
  let debuggerDisconnected = false;

  ok(gTab, "There is a gTab to use for getting a toolbox reference");
  let target = TargetFactory.forTab(gTab);

  window.addEventListener("Debugger:Shutdown", function cleanup() {
    window.removeEventListener("Debugger:Shutdown", cleanup, false);
    debuggerDisconnected = true;
    maybeFinish();
  }, false);

  let toolbox = gDevTools.getToolbox(target);
  toolbox.destroy().then(function() {
    debuggerClosed = true;
    maybeFinish();
  });

  function maybeFinish() {
    if (debuggerClosed && debuggerDisconnected) {
      (finish || aCallback)();
    }
  }
}

function get_tab_actor_for_url(aClient, aURL, aCallback) {
  aClient.listTabs(function(aResponse) {
    for each (let tab in aResponse.tabs) {
      if (tab.url == aURL) {
        aCallback(tab);
        return;
      }
    }
  });
}

function attach_tab_actor_for_url(aClient, aURL, aCallback) {
  get_tab_actor_for_url(aClient, aURL, function(actor) {
    aClient.attachTab(actor.actor, function(aResponse) {
      aCallback(actor, aResponse);
    });
  });
}

function attach_thread_actor_for_url(aClient, aURL, aCallback) {
  attach_tab_actor_for_url(aClient, aURL, function(aTabActor, aResponse) {
    aClient.attachThread(actor.threadActor, function(aResponse, aThreadClient) {
      // We don't care about the pause right now (use
      // get_actor_for_url() if you do), so resume it.
      aThreadClient.resume(function(aResponse) {
        aCallback(actor);
      });
    });
  });
}

function wait_for_connect_and_resume(aOnDebugging, aTab) {
  let target = TargetFactory.forTab(aTab);

  gDevTools.showToolbox(target, "jsdebugger").then(function(toolbox) {
    let dbg = toolbox.getCurrentPanel();

    // Wait for the initial resume...
    dbg.panelWin.gClient.addOneTimeListener("resumed", function() {
      aOnDebugging();
    });
  });
}

function debug_tab_pane(aURL, aOnDebugging, aBeforeTabAdded) {
  // Make any necessary preparations (start the debugger server etc.)
  if (aBeforeTabAdded) {
    aBeforeTabAdded();
  }

  let tab = addTab(aURL, function() {
    let debuggee = gBrowser.selectedTab.linkedBrowser.contentWindow.wrappedJSObject;
    let target = TargetFactory.forTab(gBrowser.selectedTab);

    info("Opening Debugger");
    gDevTools.showToolbox(target, "jsdebugger").then(function(toolbox) {
      let dbg = toolbox.getCurrentPanel();

      // Wait for the initial resume...
      dbg.panelWin.gClient.addOneTimeListener("resumed", function() {
        info("Debugger has started");
        dbg._view.Variables.lazyEmpty = false;
        dbg._view.Variables.lazyAppend = false;
        dbg._view.Variables.lazyExpand = false;
        aOnDebugging(tab, debuggee, dbg);
      });
    });
  });
}

function debug_remote(aURL, aOnDebugging, aBeforeTabAdded) {
  // Make any necessary preparations (start the debugger server etc.)
  if (aBeforeTabAdded) {
    aBeforeTabAdded();
  }

  let tab = addTab(aURL, function() {
    let debuggee = tab.linkedBrowser.contentWindow.wrappedJSObject;

    info("Opening Remote Debugger");
    let win = DebuggerUI.toggleRemoteDebugger();

    // Wait for the initial resume...
    win.panelWin.gClient.addOneTimeListener("resumed", function() {
      info("Remote Debugger has started");
      win._dbgwin.DebuggerView.Variables.lazyEmpty = false;
      win._dbgwin.DebuggerView.Variables.lazyAppend = false;
      win._dbgwin.DebuggerView.Variables.lazyExpand = false;
      aOnDebugging(tab, debuggee, win);
    });
  });
}

function debug_chrome(aURL, aOnClosing, aOnDebugging) {
  let tab = addTab(aURL, function() {
    let debuggee = tab.linkedBrowser.contentWindow.wrappedJSObject;

    info("Opening Browser Debugger");
    let win = DebuggerUI.toggleChromeDebugger(aOnClosing, function(process) {

      // The remote debugging process has started...
      info("Browser Debugger has started");
      aOnDebugging(tab, debuggee, process);
    });
  });
}
