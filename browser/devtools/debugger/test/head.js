/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";
const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
let tempScope = {};
Cu.import("resource:///modules/devtools/dbg-server.jsm", tempScope);
Cu.import("resource:///modules/devtools/dbg-client.jsm", tempScope);
Cu.import("resource:///modules/Services.jsm", tempScope);
let DebuggerServer = tempScope.DebuggerServer;
let DebuggerTransport = tempScope.DebuggerTransport;
let DebuggerClient = tempScope.DebuggerClient;
let Services = tempScope.Services;

const EXAMPLE_URL = "http://example.com/browser/browser/devtools/debugger/test/";

const TAB1_URL = EXAMPLE_URL + "browser_dbg_tab1.html";
const TAB2_URL = EXAMPLE_URL + "browser_dbg_tab2.html";
const STACK_URL = EXAMPLE_URL + "browser_dbg_stack.html";

if (!DebuggerServer.initialized) {
  DebuggerServer.init();
  DebuggerServer.addBrowserActors();
}

waitForExplicitFinish();

function addTab(aURL, aOnload)
{
  gBrowser.selectedTab = gBrowser.addTab(aURL);

  let tab = gBrowser.selectedTab;
  if (aOnload) {
    let handler = function() {
      if (tab.linkedBrowser.currentURI.spec == aURL) {
        tab.removeEventListener("load", handler, false);
        aOnload();
      }
    }
    tab.addEventListener("load", handler, false);
  }

  return tab;
}

function removeTab(aTab) {
  gBrowser.removeTab(aTab);
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
    aClient.request({ to: actor.actor, type: "attach" }, function(aResponse) {
      aCallback(actor, aResponse);
    });
  });
}

function attach_thread_actor_for_url(aClient, aURL, aCallback) {
  attach_tab_actor_for_url(aClient, aURL, function(aTabActor, aResponse) {
    aClient.request({ "to": actor.threadActor, "type": "attach" }, function(aResponse) {
      // We don't care about the pause right now (use
      // get_actor_for_url() if you do), so resume it.
      aClient.request({ to: actor.threadActor, type: "resume" }, function(aResponse) {
        aCallback(actor);
      });
    });
  });
}

function debug_tab_pane(aURL, aOnDebugging)
{
  let tab = addTab(aURL, function() {
    gBrowser.selectedTab = gTab;

    let debuggee = tab.linkedBrowser.contentWindow.wrappedJSObject;

    let pane = DebuggerUI.toggleDebugger();
    pane.onConnected = function() {
      // Wait for the initial resume...
      pane.debuggerWindow.gClient.addOneTimeListener("resumed", function() {
        delete pane.onConnected;
        aOnDebugging(tab, debuggee, pane);
      });
    };
  });
}
