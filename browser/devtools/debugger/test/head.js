/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

let { Services } = Cu.import("resource://gre/modules/Services.jsm", {});

// Disable logging for faster test runs. Set this pref to true if you want to
// debug a test in your try runs. Both the debugger server and frontend will
// be affected by this pref.
let gEnableLogging = Services.prefs.getBoolPref("devtools.debugger.log");
Services.prefs.setBoolPref("devtools.debugger.log", false);

let { Task } = Cu.import("resource://gre/modules/Task.jsm", {});
let { Promise: promise } = Cu.import("resource://gre/modules/devtools/deprecated-sync-thenables.js", {});
let { gDevTools } = Cu.import("resource:///modules/devtools/gDevTools.jsm", {});
let { devtools } = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
let { require } = devtools;
let { DevToolsUtils } = Cu.import("resource://gre/modules/devtools/DevToolsUtils.jsm", {});
let { BrowserToolboxProcess } = Cu.import("resource:///modules/devtools/ToolboxProcess.jsm", {});
let { DebuggerServer } = Cu.import("resource://gre/modules/devtools/dbg-server.jsm", {});
let { DebuggerClient } = Cu.import("resource://gre/modules/devtools/dbg-client.jsm", {});
let { AddonManager } = Cu.import("resource://gre/modules/AddonManager.jsm", {});
let EventEmitter = require("devtools/toolkit/event-emitter");
const { promiseInvoke } = require("devtools/async-utils");
let TargetFactory = devtools.TargetFactory;
let Toolbox = devtools.Toolbox;

const EXAMPLE_URL = "http://example.com/browser/browser/devtools/debugger/test/";
const FRAME_SCRIPT_URL = getRootDirectory(gTestPath) + "code_frame-script.js";

gDevTools.testing = true;
SimpleTest.registerCleanupFunction(() => {
  gDevTools.testing = false;
});

// All tests are asynchronous.
waitForExplicitFinish();

registerCleanupFunction(function* () {
  info("finish() was called, cleaning up...");
  Services.prefs.setBoolPref("devtools.debugger.log", gEnableLogging);

  while (gBrowser && gBrowser.tabs && gBrowser.tabs.length > 1) {
    info("Destroying toolbox.");
    let target = TargetFactory.forTab(gBrowser.selectedTab);
    yield gDevTools.closeToolbox(target);

    info("Removing tab.");
    gBrowser.removeCurrentTab();
  }

  // Properly shut down the server to avoid memory leaks.
  DebuggerServer.destroy();

  // Debugger tests use a lot of memory, so force a GC to help fragmentation.
  info("Forcing GC after debugger test.");
  Cu.forceGC();
});

// Import the GCLI test helper
let testDir = gTestPath.substr(0, gTestPath.lastIndexOf("/"));
testDir = testDir.replace(/\/\//g, '/');
testDir = testDir.replace("chrome:/mochitest", "chrome://mochitest");
let helpersjs = testDir + "/../../commandline/test/helpers.js";
Services.scriptloader.loadSubScript(helpersjs, this);

// Redeclare dbg_assert with a fatal behavior.
function dbg_assert(cond, e) {
  if (!cond) {
    throw e;
  }
}

function addWindow(aUrl) {
  info("Adding window: " + aUrl);
  return promise.resolve(getChromeWindow(window.open(aUrl)));
}

function getChromeWindow(aWindow) {
  return aWindow
    .QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIWebNavigation)
    .QueryInterface(Ci.nsIDocShellTreeItem).rootTreeItem
    .QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindow);
}

function addTab(aUrl, aWindow) {
  info("Adding tab: " + aUrl);

  let deferred = promise.defer();
  let targetWindow = aWindow || window;
  let targetBrowser = targetWindow.gBrowser;

  targetWindow.focus();
  let tab = targetBrowser.selectedTab = targetBrowser.addTab(aUrl);
  let linkedBrowser = tab.linkedBrowser;

  info("Loading frame script with url " + FRAME_SCRIPT_URL + ".");
  linkedBrowser.messageManager.loadFrameScript(FRAME_SCRIPT_URL, false);

  linkedBrowser.addEventListener("load", function onLoad() {
    linkedBrowser.removeEventListener("load", onLoad, true);
    info("Tab added and finished loading: " + aUrl);
    deferred.resolve(tab);
  }, true);

  return deferred.promise;
}

function removeTab(aTab, aWindow) {
  info("Removing tab.");

  let deferred = promise.defer();
  let targetWindow = aWindow || window;
  let targetBrowser = targetWindow.gBrowser;
  let tabContainer = targetBrowser.tabContainer;

  tabContainer.addEventListener("TabClose", function onClose(aEvent) {
    tabContainer.removeEventListener("TabClose", onClose, false);
    info("Tab removed and finished closing.");
    deferred.resolve();
  }, false);

  targetBrowser.removeTab(aTab);
  return deferred.promise;
}

function addAddon(aUrl) {
  info("Installing addon: " + aUrl);

  let deferred = promise.defer();

  AddonManager.getInstallForURL(aUrl, aInstaller => {
    aInstaller.install();
    let listener = {
      onInstallEnded: function(aAddon, aAddonInstall) {
        aInstaller.removeListener(listener);

        // Wait for add-on's startup scripts to execute. See bug 997408
        executeSoon(function() {
          deferred.resolve(aAddonInstall);
        });
      }
    };
    aInstaller.addListener(listener);
  }, "application/x-xpinstall");

  return deferred.promise;
}

function removeAddon(aAddon) {
  info("Removing addon.");

  let deferred = promise.defer();

  let listener = {
    onUninstalled: function(aUninstalledAddon) {
      if (aUninstalledAddon != aAddon) {
        return;
      }
      AddonManager.removeAddonListener(listener);
      deferred.resolve();
    }
  };
  AddonManager.addAddonListener(listener);
  aAddon.uninstall();

  return deferred.promise;
}

function getTabActorForUrl(aClient, aUrl) {
  let deferred = promise.defer();

  aClient.listTabs(aResponse => {
    let tabActor = aResponse.tabs.filter(aGrip => aGrip.url == aUrl).pop();
    deferred.resolve(tabActor);
  });

  return deferred.promise;
}

function getAddonActorForUrl(aClient, aUrl) {
  info("Get addon actor for URL: " + aUrl);
  let deferred = promise.defer();

  aClient.listAddons(aResponse => {
    let addonActor = aResponse.addons.filter(aGrip => aGrip.url == aUrl).pop();
    info("got addon actor for URL: " + addonActor.actor);
    deferred.resolve(addonActor);
  });

  return deferred.promise;
}

function attachTabActorForUrl(aClient, aUrl) {
  let deferred = promise.defer();

  getTabActorForUrl(aClient, aUrl).then(aGrip => {
    aClient.attachTab(aGrip.actor, aResponse => {
      deferred.resolve([aGrip, aResponse]);
    });
  });

  return deferred.promise;
}

function attachThreadActorForUrl(aClient, aUrl) {
  let deferred = promise.defer();

  attachTabActorForUrl(aClient, aUrl).then(([aGrip, aResponse]) => {
    aClient.attachThread(aResponse.threadActor, (aResponse, aThreadClient) => {
      aThreadClient.resume(aResponse => {
        deferred.resolve(aThreadClient);
      });
    });
  });

  return deferred.promise;
}

function once(aTarget, aEventName, aUseCapture = false) {
  info("Waiting for event: '" + aEventName + "' on " + aTarget + ".");

  let deferred = promise.defer();

  for (let [add, remove] of [
    ["addEventListener", "removeEventListener"],
    ["addListener", "removeListener"],
    ["on", "off"]
  ]) {
    if ((add in aTarget) && (remove in aTarget)) {
      aTarget[add](aEventName, function onEvent(...aArgs) {
        aTarget[remove](aEventName, onEvent, aUseCapture);
        deferred.resolve.apply(deferred, aArgs);
      }, aUseCapture);
      break;
    }
  }

  return deferred.promise;
}

function waitForTick() {
  let deferred = promise.defer();
  executeSoon(deferred.resolve);
  return deferred.promise;
}

function waitForTime(aDelay) {
  let deferred = promise.defer();
  setTimeout(deferred.resolve, aDelay);
  return deferred.promise;
}

function waitForSourceShown(aPanel, aUrl) {
  return waitForDebuggerEvents(aPanel, aPanel.panelWin.EVENTS.SOURCE_SHOWN).then(aSource => {
    let sourceUrl = aSource.url || aSource.introductionUrl;
    info("Source shown: " + sourceUrl);

    if (!sourceUrl.includes(aUrl)) {
      return waitForSourceShown(aPanel, aUrl);
    } else {
      ok(true, "The correct source has been shown.");
    }
  });
}

function waitForEditorLocationSet(aPanel) {
  return waitForDebuggerEvents(aPanel, aPanel.panelWin.EVENTS.EDITOR_LOCATION_SET);
}

function ensureSourceIs(aPanel, aUrlOrSource, aWaitFlag = false) {
  let sources = aPanel.panelWin.DebuggerView.Sources;

  if (sources.selectedValue === aUrlOrSource ||
      sources.selectedItem.attachment.source.url.includes(aUrlOrSource)) {
    ok(true, "Expected source is shown: " + aUrlOrSource);
    return promise.resolve(null);
  }
  if (aWaitFlag) {
    return waitForSourceShown(aPanel, aUrlOrSource);
  }
  ok(false, "Expected source was not already shown: " + aUrlOrSource);
  return promise.reject(null);
}

function waitForCaretUpdated(aPanel, aLine, aCol = 1) {
  return waitForEditorEvents(aPanel, "cursorActivity").then(() => {
    let cursor = aPanel.panelWin.DebuggerView.editor.getCursor();
    info("Caret updated: " + (cursor.line + 1) + ", " + (cursor.ch + 1));

    if (!isCaretPos(aPanel, aLine, aCol)) {
      return waitForCaretUpdated(aPanel, aLine, aCol);
    } else {
      ok(true, "The correct caret position has been set.");
    }
  });
}

function ensureCaretAt(aPanel, aLine, aCol = 1, aWaitFlag = false) {
  if (isCaretPos(aPanel, aLine, aCol)) {
    ok(true, "Expected caret position is set: " + aLine + "," + aCol);
    return promise.resolve(null);
  }
  if (aWaitFlag) {
    return waitForCaretUpdated(aPanel, aLine, aCol);
  }
  ok(false, "Expected caret position was not already set: " + aLine + "," + aCol);
  return promise.reject(null);
}

function isCaretPos(aPanel, aLine, aCol = 1) {
  let editor = aPanel.panelWin.DebuggerView.editor;
  let cursor = editor.getCursor();

  // Source editor starts counting line and column numbers from 0.
  info("Current editor caret position: " + (cursor.line + 1) + ", " + (cursor.ch + 1));
  return cursor.line == (aLine - 1) && cursor.ch == (aCol - 1);
}

function isDebugPos(aPanel, aLine) {
  let editor = aPanel.panelWin.DebuggerView.editor;
  let location = editor.getDebugLocation();

  // Source editor starts counting line and column numbers from 0.
  info("Current editor debug position: " + (location + 1));
  return location != null && editor.hasLineClass(aLine - 1, "debug-line");
}

function isEditorSel(aPanel, [start, end]) {
  let editor = aPanel.panelWin.DebuggerView.editor;
  let range = {
    start: editor.getOffset(editor.getCursor("start")),
    end:   editor.getOffset(editor.getCursor())
  };

  // Source editor starts counting line and column numbers from 0.
  info("Current editor selection: " + (range.start + 1) + ", " + (range.end + 1));
  return range.start == (start - 1) && range.end == (end - 1);
}

function waitForSourceAndCaret(aPanel, aUrl, aLine, aCol) {
  return promise.all([
    waitForSourceShown(aPanel, aUrl),
    waitForCaretUpdated(aPanel, aLine, aCol)
  ]);
}

function waitForCaretAndScopes(aPanel, aLine, aCol) {
  return promise.all([
    waitForCaretUpdated(aPanel, aLine, aCol),
    waitForDebuggerEvents(aPanel, aPanel.panelWin.EVENTS.FETCHED_SCOPES)
  ]);
}

function waitForSourceAndCaretAndScopes(aPanel, aUrl, aLine, aCol) {
  return promise.all([
    waitForSourceAndCaret(aPanel, aUrl, aLine, aCol),
    waitForDebuggerEvents(aPanel, aPanel.panelWin.EVENTS.FETCHED_SCOPES)
  ]);
}

function waitForDebuggerEvents(aPanel, aEventName, aEventRepeat = 1) {
  info("Waiting for debugger event: '" + aEventName + "' to fire: " + aEventRepeat + " time(s).");

  let deferred = promise.defer();
  let panelWin = aPanel.panelWin;
  let count = 0;

  panelWin.on(aEventName, function onEvent(aEventName, ...aArgs) {
    info("Debugger event '" + aEventName + "' fired: " + (++count) + " time(s).");

    if (count == aEventRepeat) {
      ok(true, "Enough '" + aEventName + "' panel events have been fired.");
      panelWin.off(aEventName, onEvent);
      deferred.resolve.apply(deferred, aArgs);
    }
  });

  return deferred.promise;
}

function waitForEditorEvents(aPanel, aEventName, aEventRepeat = 1) {
  info("Waiting for editor event: '" + aEventName + "' to fire: " + aEventRepeat + " time(s).");

  let deferred = promise.defer();
  let editor = aPanel.panelWin.DebuggerView.editor;
  let count = 0;

  editor.on(aEventName, function onEvent(...aArgs) {
    info("Editor event '" + aEventName + "' fired: " + (++count) + " time(s).");

    if (count == aEventRepeat) {
      ok(true, "Enough '" + aEventName + "' editor events have been fired.");
      editor.off(aEventName, onEvent);
      deferred.resolve.apply(deferred, aArgs);
    }
  });

  return deferred.promise;
}

function waitForThreadEvents(aPanel, aEventName, aEventRepeat = 1) {
  info("Waiting for thread event: '" + aEventName + "' to fire: " + aEventRepeat + " time(s).");

  let deferred = promise.defer();
  let thread = aPanel.panelWin.gThreadClient;
  let count = 0;

  thread.addListener(aEventName, function onEvent(aEventName, ...aArgs) {
    info("Thread event '" + aEventName + "' fired: " + (++count) + " time(s).");

    if (count == aEventRepeat) {
      ok(true, "Enough '" + aEventName + "' thread events have been fired.");
      thread.removeListener(aEventName, onEvent);
      deferred.resolve.apply(deferred, aArgs);
    }
  });

  return deferred.promise;
}

function waitForClientEvents(aPanel, aEventName, aEventRepeat = 1) {
  info("Waiting for client event: '" + aEventName + "' to fire: " + aEventRepeat + " time(s).");

  let deferred = promise.defer();
  let client = aPanel.panelWin.gClient;
  let count = 0;

  client.addListener(aEventName, function onEvent(aEventName, ...aArgs) {
    info("Thread event '" + aEventName + "' fired: " + (++count) + " time(s).");

    if (count == aEventRepeat) {
      ok(true, "Enough '" + aEventName + "' thread events have been fired.");
      client.removeListener(aEventName, onEvent);
      deferred.resolve.apply(deferred, aArgs);
    }
  });

  return deferred.promise;
}

function waitForClipboardPromise(setup, expected) {
  return new Promise((resolve, reject) => {
    SimpleTest.waitForClipboard(expected, setup, resolve, reject);
  });
}

function ensureThreadClientState(aPanel, aState) {
  let thread = aPanel.panelWin.gThreadClient;
  let state = thread.state;

  info("Thread is: '" + state + "'.");

  if (state == aState) {
    return promise.resolve(null);
  } else {
    return waitForThreadEvents(aPanel, aState);
  }
}

function navigateActiveTabTo(aPanel, aUrl, aWaitForEventName, aEventRepeat) {
  let finished = waitForDebuggerEvents(aPanel, aWaitForEventName, aEventRepeat);
  let activeTab = aPanel.panelWin.DebuggerController._target.activeTab;
  aUrl ? activeTab.navigateTo(aUrl) : activeTab.reload();
  return finished;
}

function navigateActiveTabInHistory(aPanel, aDirection, aWaitForEventName, aEventRepeat) {
  let finished = waitForDebuggerEvents(aPanel, aWaitForEventName, aEventRepeat);
  content.history[aDirection]();
  return finished;
}

function reloadActiveTab(aPanel, aWaitForEventName, aEventRepeat) {
  return navigateActiveTabTo(aPanel, null, aWaitForEventName, aEventRepeat);
}

function clearText(aElement) {
  info("Clearing text...");
  aElement.focus();
  aElement.value = "";
}

function setText(aElement, aText) {
  clearText(aElement);
  info("Setting text: " + aText);
  aElement.value = aText;
}

function typeText(aElement, aText) {
  info("Typing text: " + aText);
  aElement.focus();
  EventUtils.sendString(aText, aElement.ownerDocument.defaultView);
}

function backspaceText(aElement, aTimes) {
  info("Pressing backspace " + aTimes + " times.");
  for (let i = 0; i < aTimes; i++) {
    aElement.focus();
    EventUtils.sendKey("BACK_SPACE", aElement.ownerDocument.defaultView);
  }
}

function getTab(aTarget, aWindow) {
  if (aTarget instanceof XULElement) {
    return promise.resolve(aTarget);
  } else {
    return addTab(aTarget, aWindow);
  }
}

function getSources(aClient) {
  let deferred = promise.defer();

  aClient.getSources(({sources}) => deferred.resolve(sources));

  return deferred.promise;
}

function initDebugger(aTarget, aWindow) {
  info("Initializing a debugger panel.");

  return getTab(aTarget, aWindow).then(aTab => {
    info("Debugee tab added successfully: " + aTarget);

    let deferred = promise.defer();
    let debuggee = aTab.linkedBrowser.contentWindow.wrappedJSObject;
    let target = TargetFactory.forTab(aTab);

    gDevTools.showToolbox(target, "jsdebugger").then(aToolbox => {
      info("Debugger panel shown successfully.");

      let debuggerPanel = aToolbox.getCurrentPanel();
      let panelWin = debuggerPanel.panelWin;

      // Wait for the initial resume...
      panelWin.gClient.addOneTimeListener("resumed", () => {
        info("Debugger client resumed successfully.");

        prepareDebugger(debuggerPanel);
        deferred.resolve([aTab, debuggee, debuggerPanel, aWindow]);
      });
    });

    return deferred.promise;
  });
}

// Creates an add-on debugger for a given add-on. The returned AddonDebugger
// object must be destroyed before finishing the test
function initAddonDebugger(aUrl) {
  let addonDebugger = new AddonDebugger();
  return addonDebugger.init(aUrl).then(() => addonDebugger);
}

function AddonDebugger() {
  this._onMessage = this._onMessage.bind(this);
  this._onConsoleAPICall = this._onConsoleAPICall.bind(this);
  EventEmitter.decorate(this);
}

AddonDebugger.prototype = {
  init: Task.async(function*(aUrl) {
    info("Initializing an addon debugger panel.");

    if (!DebuggerServer.initialized) {
      DebuggerServer.init();
      DebuggerServer.addBrowserActors();
    }
    DebuggerServer.allowChromeProcess = true;

    this.frame = document.createElement("iframe");
    this.frame.setAttribute("height", 400);
    document.documentElement.appendChild(this.frame);
    window.addEventListener("message", this._onMessage);

    let transport = DebuggerServer.connectPipe();
    this.client = new DebuggerClient(transport);

    let connected = promise.defer();
    this.client.connect(connected.resolve);
    yield connected.promise;

    let addonActor = yield getAddonActorForUrl(this.client, aUrl);

    let targetOptions = {
      form: addonActor,
      client: this.client,
      chrome: true,
      isTabActor: false
    };

    let toolboxOptions = {
      customIframe: this.frame
    };

    this.target = devtools.TargetFactory.forTab(targetOptions);
    let toolbox = yield gDevTools.showToolbox(this.target, "jsdebugger", devtools.Toolbox.HostType.CUSTOM, toolboxOptions);

    info("Addon debugger panel shown successfully.");

    this.debuggerPanel = toolbox.getCurrentPanel();

    // Wait for the initial resume...
    yield waitForClientEvents(this.debuggerPanel, "resumed");
    yield prepareDebugger(this.debuggerPanel);
    yield this._attachConsole();
  }),

  destroy: Task.async(function*() {
    let deferred = promise.defer();
    this.client.close(deferred.resolve);
    yield deferred.promise;
    yield this.debuggerPanel._toolbox.destroy();
    this.frame.remove();
    window.removeEventListener("message", this._onMessage);
  }),

  _attachConsole: function() {
    let deferred = promise.defer();
    this.client.attachConsole(this.target.form.consoleActor, ["ConsoleAPI"], (aResponse, aWebConsoleClient) => {
      if (aResponse.error) {
        deferred.reject(aResponse);
      }
      else {
        this.webConsole = aWebConsoleClient;
        this.client.addListener("consoleAPICall", this._onConsoleAPICall);
        deferred.resolve();
      }
    });
    return deferred.promise;
  },

  _onConsoleAPICall: function(aType, aPacket) {
    if (aPacket.from != this.webConsole.actor)
      return;
    this.emit("console", aPacket.message);
  },

  /**
   * Returns a list of the groups and sources in the UI. The returned array
   * contains objects for each group with properties name and sources. The
   * sources property contains an array with objects for each source for that
   * group with properties label and url.
   */
  getSourceGroups: Task.async(function*() {
    let debuggerWin = this.debuggerPanel.panelWin;
    let sources = yield getSources(debuggerWin.gThreadClient);
    ok(sources.length, "retrieved sources");

    // groups will be the return value, groupmap and the maps we put in it will
    // be used as quick lookups to add the url information in below
    let groups = [];
    let groupmap = new Map();

    let uigroups = this.debuggerPanel.panelWin.document.querySelectorAll(".side-menu-widget-group");
    for (let g of uigroups) {
      let name = g.querySelector(".side-menu-widget-group-title .name").value;
      let group = {
        name: name,
        sources: []
      };
      groups.push(group);
      let labelmap = new Map();
      groupmap.set(name, labelmap);

      for (let l of g.querySelectorAll(".dbg-source-item")) {
        let source = {
          label: l.value,
          url: null
        };

        labelmap.set(l.value, source);
        group.sources.push(source);
      }
    }

    for (let source of sources) {
      let { label, group } = debuggerWin.DebuggerView.Sources.getItemByValue(source.actor).attachment;

      if (!groupmap.has(group)) {
        ok(false, "Saw a source group not in the UI: " + group);
        continue;
      }

      if (!groupmap.get(group).has(label)) {
        ok(false, "Saw a source label not in the UI: " + label);
        continue;
      }

      groupmap.get(group).get(label).url = source.url.split(" -> ").pop();
    }

    return groups;
  }),

  _onMessage: function(event) {
    let json = JSON.parse(event.data);
    switch (json.name) {
      case "toolbox-title":
        this.title = json.data.value;
        break;
    }
  }
}

function initChromeDebugger(aOnClose) {
  info("Initializing a chrome debugger process.");

  let deferred = promise.defer();

  // Wait for the toolbox process to start...
  BrowserToolboxProcess.init(aOnClose, (aEvent, aProcess) => {
    info("Browser toolbox process started successfully.");

    prepareDebugger(aProcess);
    deferred.resolve(aProcess);
  });

  return deferred.promise;
}

function prepareDebugger(aDebugger) {
  if ("target" in aDebugger) {
    let view = aDebugger.panelWin.DebuggerView;
    view.Variables.lazyEmpty = false;
    view.Variables.lazySearch = false;
    view.Filtering.FilteredSources._autoSelectFirstItem = true;
    view.Filtering.FilteredFunctions._autoSelectFirstItem = true;
  } else {
    // Nothing to do here yet.
  }
}

function teardown(aPanel, aFlags = {}) {
  info("Destroying the specified debugger.");

  let toolbox = aPanel._toolbox;
  let tab = aPanel.target.tab;
  let debuggerRootActorDisconnected = once(window, "Debugger:Shutdown");
  let debuggerPanelDestroyed = once(aPanel, "destroyed");
  let devtoolsToolboxDestroyed = toolbox.destroy();

  return promise.all([
    debuggerRootActorDisconnected,
    debuggerPanelDestroyed,
    devtoolsToolboxDestroyed
  ]).then(() => aFlags.noTabRemoval ? null : removeTab(tab));
}

function closeDebuggerAndFinish(aPanel, aFlags = {}) {
  let thread = aPanel.panelWin.gThreadClient;
  if (thread.state == "paused" && !aFlags.whilePaused) {
    ok(false, "You should use 'resumeDebuggerThenCloseAndFinish' instead, " +
              "unless you're absolutely sure about what you're doing.");
  }
  return teardown(aPanel, aFlags).then(finish);
}

function resumeDebuggerThenCloseAndFinish(aPanel, aFlags = {}) {
  let deferred = promise.defer();
  let thread = aPanel.panelWin.gThreadClient;
  thread.resume(() => closeDebuggerAndFinish(aPanel, aFlags).then(deferred.resolve));
  return deferred.promise;
}

// Blackboxing helpers

function getBlackBoxButton(aPanel) {
  return aPanel.panelWin.document.getElementById("black-box");
}

/**
 * Returns the node that has the black-boxed class applied to it.
 */
function getSelectedSourceElement(aPanel) {
    return aPanel.panelWin.DebuggerView.Sources.selectedItem.prebuiltNode;
}

function toggleBlackBoxing(aPanel, aSource = null) {
  function clickBlackBoxButton() {
    getBlackBoxButton(aPanel).click();
  }

  const blackBoxChanged = waitForThreadEvents(aPanel, "blackboxchange");

  if (aSource) {
    aPanel.panelWin.DebuggerView.Sources.selectedValue = aSource;
    ensureSourceIs(aPanel, aSource, true).then(clickBlackBoxButton);
  } else {
    clickBlackBoxButton();
  }

  return blackBoxChanged;
}

function selectSourceAndGetBlackBoxButton(aPanel, aUrl) {
  function returnBlackboxButton() {
    return getBlackBoxButton(aPanel);
  }

  let sources = aPanel.panelWin.DebuggerView.Sources;
  sources.selectedValue = getSourceActor(sources, aUrl);
  return ensureSourceIs(aPanel, aUrl, true).then(returnBlackboxButton);
}

// Variables view inspection popup helpers

function openVarPopup(aPanel, aCoords, aWaitForFetchedProperties) {
  let events = aPanel.panelWin.EVENTS;
  let editor = aPanel.panelWin.DebuggerView.editor;
  let bubble = aPanel.panelWin.DebuggerView.VariableBubble;
  let tooltip = bubble._tooltip.panel;

  let popupShown = once(tooltip, "popupshown");
  let fetchedProperties = aWaitForFetchedProperties
    ? waitForDebuggerEvents(aPanel, events.FETCHED_BUBBLE_PROPERTIES)
    : promise.resolve(null);
  let updatedFrame = waitForDebuggerEvents(aPanel, events.FETCHED_SCOPES);

  let { left, top } = editor.getCoordsFromPosition(aCoords);
  bubble._findIdentifier(left, top);
  return promise.all([popupShown, fetchedProperties, updatedFrame]).then(waitForTick);
}

// Simulates the mouse hovering a variable in the debugger
// Takes in account the position of the cursor in the text, if the text is
// selected and if a button is currently pushed (aButtonPushed > 0).
// The function returns a promise which returns true if the popup opened or
// false if it didn't
function intendOpenVarPopup(aPanel, aPosition, aButtonPushed) {
  let bubble = aPanel.panelWin.DebuggerView.VariableBubble;
  let editor = aPanel.panelWin.DebuggerView.editor;
  let tooltip = bubble._tooltip;

  let { left, top } = editor.getCoordsFromPosition(aPosition);

  const eventDescriptor = {
    clientX: left,
    clientY: top,
    buttons: aButtonPushed
  };

  bubble._onMouseMove(eventDescriptor);

  const deferred = promise.defer();
  window.setTimeout(
    function() {
      if(tooltip.isEmpty()) {
        deferred.resolve(false);
      } else {
        deferred.resolve(true);
      }
    },
    tooltip.defaultShowDelay + 1000
  );

  return deferred.promise;
}

function hideVarPopup(aPanel) {
  let bubble = aPanel.panelWin.DebuggerView.VariableBubble;
  let tooltip = bubble._tooltip.panel;

  let popupHiding = once(tooltip, "popuphiding");
  bubble.hideContents();
  return popupHiding.then(waitForTick);
}

function hideVarPopupByScrollingEditor(aPanel) {
  let editor = aPanel.panelWin.DebuggerView.editor;
  let bubble = aPanel.panelWin.DebuggerView.VariableBubble;
  let tooltip = bubble._tooltip.panel;

  let popupHiding = once(tooltip, "popuphiding");
  editor.setFirstVisibleLine(0);
  return popupHiding.then(waitForTick);
}

function reopenVarPopup(...aArgs) {
  return hideVarPopup.apply(this, aArgs).then(() => openVarPopup.apply(this, aArgs));
}

// Tracing helpers

function startTracing(aPanel) {
  const deferred = promise.defer();
  aPanel.panelWin.DebuggerController.Tracer.startTracing(aResponse => {
    if (aResponse.error) {
      deferred.reject(aResponse);
    } else {
      deferred.resolve(aResponse);
    }
  });
  return deferred.promise;
}

function stopTracing(aPanel) {
  const deferred = promise.defer();
  aPanel.panelWin.DebuggerController.Tracer.stopTracing(aResponse => {
    if (aResponse.error) {
      deferred.reject(aResponse);
    } else {
      deferred.resolve(aResponse);
    }
  });
  return deferred.promise;
}

function filterTraces(aPanel, f) {
  const traces = aPanel.panelWin.document
    .getElementById("tracer-traces")
    .querySelector("scrollbox")
    .children;
  return Array.filter(traces, f);
}
function attachAddonActorForUrl(aClient, aUrl) {
  let deferred = promise.defer();

  getAddonActorForUrl(aClient, aUrl).then(aGrip => {
    aClient.attachAddon(aGrip.actor, aResponse => {
      deferred.resolve([aGrip, aResponse]);
    });
  });

  return deferred.promise;
}

function rdpInvoke(aClient, aMethod, ...args) {
  return promiseInvoke(aClient, aMethod, ...args)
    .then(({error, message }) => {
      if (error) {
        throw new Error(error + ": " + message);
      }
    });
}

function doResume(aPanel) {
  const threadClient = aPanel.panelWin.gThreadClient;
  return rdpInvoke(threadClient, threadClient.resume);
}

function doInterrupt(aPanel) {
  const threadClient = aPanel.panelWin.gThreadClient;
  return rdpInvoke(threadClient, threadClient.interrupt);
}

function pushPrefs(...aPrefs) {
  let deferred = promise.defer();
  SpecialPowers.pushPrefEnv({"set": aPrefs}, deferred.resolve);
  return deferred.promise;
}

function popPrefs() {
  let deferred = promise.defer();
  SpecialPowers.popPrefEnv(deferred.resolve);
  return deferred.promise;
}

function sendMessageToTab(tab, name, data, objects) {
  info("Sending message with name " + name + " to tab.");

  tab.linkedBrowser.messageManager.sendAsyncMessage(name, data, objects);
}

function waitForMessageFromTab(tab, name) {
  info("Waiting for message with name " + name + " from tab.");

  return new Promise(function (resolve) {
    let messageManager = tab.linkedBrowser.messageManager;
    messageManager.addMessageListener(name, function listener(message) {
      messageManager.removeMessageListener(name, listener);
      resolve(message);
    });
  });
}

function callInTab(tab, name) {
  info("Calling function with name " + name + " in tab.");

  sendMessageToTab(tab, "test:call", {
    name: name,
    args: Array.prototype.slice.call(arguments, 2)
  });
  waitForMessageFromTab(tab, "test:call");
}

function evalInTab(tab, string) {
  info("Evalling string " + string + " in tab.");

  sendMessageToTab(tab, "test:eval", {
    string: string,
  });
  waitForMessageFromTab(tab, "test:eval");
}

function sendMouseClickToTab(tab, target) {
  info("Sending mouse click to tab.");

  sendMessageToTab(tab, "test:click", undefined, {
    target: target
  });
}

// Source helpers

function getSelectedSourceURL(aSources) {
  return (aSources.selectedItem &&
          aSources.selectedItem.attachment.source.url);
}

function getSourceURL(aSources, aActor) {
  let item = aSources.getItemByValue(aActor);
  return item && item.attachment.source.url;
}

function getSourceActor(aSources, aURL) {
  let item = aSources.getItemForAttachment(a => a.source.url === aURL);
  return item && item.value;
}

function getSourceForm(aSources, aURL) {
  let item = aSources.getItemByValue(getSourceActor(gSources, aURL));
  return item.attachment.source;
}

function connect(client) {
  info("Connecting client.");
  return new Promise(function (resolve) {
    client.connect(function () {
      resolve();
    });
  });
}

function close(client) {
  info("Closing client.\n");
  return new Promise(function (resolve) {
    client.close(() => {
      resolve();
    });
  });
}

function listTabs(client) {
  info("Listing tabs.");
  return new Promise(function (resolve) {
    client.listTabs(function (response) {
      resolve(response);
    });
  });
}

function findTab(tabs, url) {
  info("Finding tab with url '" + url + "'.");
  for (let tab of tabs) {
    if (tab.url === url) {
      return tab;
    }
  }
  return null;
}

function attachTab(client, tab) {
  info("Attaching to tab with url '" + tab.url + "'.");
  return new Promise(function (resolve) {
    client.attachTab(tab.actor, function (response, tabClient) {
      resolve([response, tabClient]);
    });
  });
}

function listWorkers(tabClient) {
  info("Listing workers.");
  return new Promise(function (resolve) {
    tabClient.listWorkers(function (response) {
      resolve(response);
    });
  });
}

function waitForWorkerListChanged(tabClient) {
  info("Waiting for worker list to change.");
  return new Promise(function (resolve) {
    tabClient.addListener("workerListChanged", function listener() {
      tabClient.removeListener("workerListChanged", listener);
      resolve();
    });
  });
}
