/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* import-globals-from ../../shared/test/shared-head.js */
/* import-globals-from ../../shared/test/telemetry-test-helpers.js */

// shared-head.js handles imports, constants, and utility functions
Services.scriptloader.loadSubScript("chrome://mochitests/content/browser/devtools/client/shared/test/shared-head.js", this);

const EventEmitter = require("devtools/shared/event-emitter");

function toggleAllTools(state) {
  for (let [, tool] of gDevTools._tools) {
    if (!tool.visibilityswitch) {
      continue;
    }
    if (state) {
      Services.prefs.setBoolPref(tool.visibilityswitch, true);
    } else {
      Services.prefs.clearUserPref(tool.visibilityswitch);
    }
  }
}

function getChromeActors(callback) {
  let { DebuggerServer } = require("devtools/server/main");
  let { DebuggerClient } = require("devtools/shared/client/debugger-client");

  DebuggerServer.init();
  DebuggerServer.registerAllActors();
  DebuggerServer.allowChromeProcess = true;

  let client = new DebuggerClient(DebuggerServer.connectPipe());
  client.connect()
    .then(() => client.getProcess())
    .then(response => {
      callback(client, response.form);
    });

  SimpleTest.registerCleanupFunction(() => {
    DebuggerServer.destroy();
  });
}

function getSourceActor(aSources, aURL) {
  let item = aSources.getItemForAttachment(a => a.source.url === aURL);
  return item && item.value;
}

/**
 * Open a Scratchpad window.
 *
 * @return nsIDOMWindow
 *         The new window object that holds Scratchpad.
 */
async function openScratchpadWindow() {
  let { promise: p, resolve } = defer();
  let win = ScratchpadManager.openScratchpad();

  await once(win, "load");

  win.Scratchpad.addObserver({
    onReady: function() {
      win.Scratchpad.removeObserver(this);
      resolve(win);
    }
  });
  return p;
}

/**
 * Wait for a content -> chrome message on the message manager (the window
 * messagemanager is used).
 * @param {String} name The message name
 * @return {Promise} A promise that resolves to the response data when the
 * message has been received
 */
function waitForContentMessage(name) {
  info("Expecting message " + name + " from content");

  let mm = gBrowser.selectedBrowser.messageManager;

  let def = defer();
  mm.addMessageListener(name, function onMessage(msg) {
    mm.removeMessageListener(name, onMessage);
    def.resolve(msg.data);
  });
  return def.promise;
}

/**
 * Send an async message to the frame script (chrome -> content) and wait for a
 * response message with the same name (content -> chrome).
 * @param {String} name The message name. Should be one of the messages defined
 * in doc_frame_script.js
 * @param {Object} data Optional data to send along
 * @param {Object} objects Optional CPOW objects to send along
 * @param {Boolean} expectResponse If set to false, don't wait for a response
 * with the same name from the content script. Defaults to true.
 * @return {Promise} Resolves to the response data if a response is expected,
 * immediately resolves otherwise
 */
function executeInContent(name, data = {}, objects = {}, expectResponse = true) {
  info("Sending message " + name + " to content");
  let mm = gBrowser.selectedBrowser.messageManager;

  mm.sendAsyncMessage(name, data, objects);
  if (expectResponse) {
    return waitForContentMessage(name);
  }
  return promise.resolve();
}

/**
 * Synthesize a keypress from a <key> element, taking into account
 * any modifiers.
 * @param {Element} el the <key> element to synthesize
 */
function synthesizeKeyElement(el) {
  let key = el.getAttribute("key") || el.getAttribute("keycode");
  let mod = {};
  el.getAttribute("modifiers").split(" ").forEach((m) => mod[m + "Key"] = true);
  info(`Synthesizing: key=${key}, mod=${JSON.stringify(mod)}`);
  EventUtils.synthesizeKey(key, mod, el.ownerDocument.defaultView);
}

/* Check the toolbox host type and prefs to make sure they match the
 * expected values
 * @param {Toolbox}
 * @param {HostType} hostType
 *        One of {SIDE, BOTTOM, WINDOW} from Toolbox.HostType
 * @param {HostType} Optional previousHostType
 *        The host that will be switched to when calling switchToPreviousHost
 */
function checkHostType(toolbox, hostType, previousHostType) {
  is(toolbox.hostType, hostType, "host type is " + hostType);

  let pref = Services.prefs.getCharPref("devtools.toolbox.host");
  is(pref, hostType, "host pref is " + hostType);

  if (previousHostType) {
    is(Services.prefs.getCharPref("devtools.toolbox.previousHost"),
      previousHostType, "The previous host is correct");
  }
}

/**
 * Create a new <script> referencing URL.  Return a promise that
 * resolves when this has happened
 * @param {String} url
 *        the url
 * @return {Promise} a promise that resolves when the element has been created
 */
function createScript(url) {
  info(`Creating script: ${url}`);
  // This is not ideal if called multiple times, as it loads the frame script
  // separately each time.  See bug 1443680.
  loadFrameScriptUtils();
  let command = `
    let script = document.createElement("script");
    script.setAttribute("src", "${url}");
    document.body.appendChild(script);
    null;
  `;
  return evalInDebuggee(command);
}

/**
 * Wait for the toolbox to notice that a given source is loaded
 * @param {Toolbox} toolbox
 * @param {String} url
 *        the url to wait for
 * @return {Promise} a promise that is resolved when the source is loaded
 */
function waitForSourceLoad(toolbox, url) {
  info(`Waiting for source ${url} to be available...`);
  return new Promise(resolve => {
    let target = toolbox.target;

    function sourceHandler(sourceEvent) {
      if (sourceEvent && sourceEvent.source && sourceEvent.source.url === url) {
        resolve();
        target.off("source-updated", sourceHandler);
      }
    }

    target.on("source-updated", sourceHandler);
  });
}

/**
* When a Toolbox is started it creates a DevToolPanel for each of the tools
* by calling toolDefinition.build(). The returned object should
* at least implement these functions. They will be used by the ToolBox.
*
* There may be no benefit in doing this as an abstract type, but if nothing
* else gives us a place to write documentation.
*/
function DevToolPanel(iframeWindow, toolbox) {
  EventEmitter.decorate(this);

  this._toolbox = toolbox;
  this._window = iframeWindow;
}

DevToolPanel.prototype = {
  open: function() {
    let deferred = defer();

    executeSoon(() => {
      this._isReady = true;
      this.emit("ready");
      deferred.resolve(this);
    });

    return deferred.promise;
  },

  get document() {
    return this._window.document;
  },

  get target() {
    return this._toolbox.target;
  },

  get toolbox() {
    return this._toolbox;
  },

  get isReady() {
    return this._isReady;
  },

  _isReady: false,

  destroy: function() {
    return defer(null);
  },
};

/**
 * Create a simple devtools test panel that implements the minimum API needed to be
 * registered and opened in the toolbox.
 */
function createTestPanel(iframeWindow, toolbox) {
  return new DevToolPanel(iframeWindow, toolbox);
}

async function openChevronMenu(toolbox) {
  let chevronMenuButton = toolbox.doc.querySelector(".tools-chevron-menu");
  EventUtils.synthesizeMouseAtCenter(chevronMenuButton, {}, toolbox.win);

  let menuPopup = toolbox.doc.querySelector("#tools-chevron-menupopup");
  ok(menuPopup, "tools-chevron-menupopup is available");

  info("Waiting for the menu popup to be displayed");
  await waitUntil(() => menuPopup && menuPopup.state === "open");

  return menuPopup;
}

function prepareToolTabReorderTest(toolbox, startingOrder) {
  Services.prefs.setCharPref("devtools.toolbox.tabsOrder", startingOrder.join(","));
  ok(!toolbox.doc.getElementById("tools-chevron-menu-button"),
     "The size of the screen being too small");

  for (const id of startingOrder) {
    ok(getElementByToolIdOrExtensionIdOrSelector(toolbox, id),
       `Tab element should exist for ${ id }`);
  }
}

async function dndToolTab(toolbox, dragTarget, dropTarget, passedTargets = []) {
  info(`Drag ${ dragTarget } to ${ dropTarget }`);
  const dragTargetEl = getElementByToolIdOrExtensionIdOrSelector(toolbox, dragTarget);

  const onReady = dragTargetEl.classList.contains("selected")
                ? Promise.resolve() : toolbox.once("select");
  EventUtils.synthesizeMouseAtCenter(dragTargetEl,
                                     { type: "mousedown" },
                                     dragTargetEl.ownerGlobal);
  await onReady;

  for (const passedTarget of passedTargets) {
    info(`Via ${ passedTarget }`);
    const passedTargetEl =
      getElementByToolIdOrExtensionIdOrSelector(toolbox, passedTarget);
    EventUtils.synthesizeMouseAtCenter(passedTargetEl,
                                       { type: "mousemove" },
                                       passedTargetEl.ownerGlobal);
  }

  if (dropTarget) {
    const dropTargetEl = getElementByToolIdOrExtensionIdOrSelector(toolbox, dropTarget);
    EventUtils.synthesizeMouseAtCenter(dropTargetEl,
                                       { type: "mousemove" },
                                       dropTargetEl.ownerGlobal);
    EventUtils.synthesizeMouseAtCenter(dropTargetEl,
                                       { type: "mouseup" },
                                       dropTargetEl.ownerGlobal);
  } else {
    const containerEl = toolbox.doc.getElementById("toolbox-container");
    EventUtils.synthesizeMouse(containerEl, 0, 0,
                               { type: "mouseout" }, containerEl.ownerGlobal);
  }
}

function assertToolTabOrder(toolbox, expectedOrder) {
  info("Check the order of the tabs on the toolbar");

  const tabEls = toolbox.doc.querySelectorAll(".devtools-tab");

  for (let i = 0; i < expectedOrder.length; i++) {
    const isOrdered = tabEls[i].dataset.id === expectedOrder[i] ||
                      tabEls[i].dataset.extensionId === expectedOrder[i];
    ok(isOrdered, `The tab[${ expectedOrder[i] }] should exist at [${ i }]`);
  }
}

function assertToolTabSelected(toolbox, dragTarget) {
  info("Check whether the drag target was selected");
  const dragTargetEl = getElementByToolIdOrExtensionIdOrSelector(toolbox, dragTarget);
  ok(dragTargetEl.classList.contains("selected"), "The dragged tool should be selected");
}

function assertToolTabPreferenceOrder(expectedOrder) {
  info("Check the order in DevTools preference for tabs order");
  is(Services.prefs.getCharPref("devtools.toolbox.tabsOrder"), expectedOrder.join(","),
     "The preference should be correct");
}

function getElementByToolIdOrExtensionIdOrSelector(toolbox, idOrSelector) {
  for (const tabEl of toolbox.doc.querySelectorAll(".devtools-tab")) {
    if (tabEl.dataset.id === idOrSelector ||
        tabEl.dataset.extensionId === idOrSelector) {
      return tabEl;
    }
  }

  return toolbox.doc.querySelector(idOrSelector);
}

function getWindow(toolbox) {
  return toolbox.win.parent;
}

async function resizeWindow(toolbox, width, height) {
  const hostWindow = toolbox.win.parent;
  const originalWidth = hostWindow.outerWidth;
  const originalHeight = hostWindow.outerHeight;
  const toWidth = width || originalWidth;
  const toHeight = height || originalHeight;

  const onResize = once(hostWindow, "resize");
  hostWindow.resizeTo(toWidth, toHeight);
  await onResize;
}
