/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* import-globals-from ../../shared/test/shared-head.js */
/* import-globals-from ../../shared/test/telemetry-test-helpers.js */

// shared-head.js handles imports, constants, and utility functions
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/shared/test/shared-head.js",
  this
);
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/inspector/test/shared-head.js",
  this
);

const EventEmitter = require("devtools/shared/event-emitter");

function toggleAllTools(state) {
  for (const [, tool] of gDevTools._tools) {
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

async function getParentProcessActors(callback) {
  const { DevToolsServer } = require("devtools/server/devtools-server");
  const { DevToolsClient } = require("devtools/client/devtools-client");

  DevToolsServer.init();
  DevToolsServer.registerAllActors();
  DevToolsServer.allowChromeProcess = true;

  SimpleTest.registerCleanupFunction(() => {
    DevToolsServer.destroy();
  });

  const client = new DevToolsClient(DevToolsServer.connectPipe());
  await client.connect();
  const mainProcessDescriptor = await client.mainRoot.getMainProcess();
  const mainProcessTargetFront = await mainProcessDescriptor.getTarget();

  callback(client, mainProcessTargetFront);
}

function getSourceActor(aSources, aURL) {
  const item = aSources.getItemForAttachment(a => a.source.url === aURL);
  return item && item.value;
}

/**
 * Synthesize a keypress from a <key> element, taking into account
 * any modifiers.
 * @param {Element} el the <key> element to synthesize
 */
function synthesizeKeyElement(el) {
  const key = el.getAttribute("key") || el.getAttribute("keycode");
  const mod = {};
  el.getAttribute("modifiers")
    .split(" ")
    .forEach(m => (mod[m + "Key"] = true));
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

  const pref = Services.prefs.getCharPref("devtools.toolbox.host");
  is(pref, hostType, "host pref is " + hostType);

  if (previousHostType) {
    is(
      Services.prefs.getCharPref("devtools.toolbox.previousHost"),
      previousHostType,
      "The previous host is correct"
    );
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
  return SpecialPowers.spawn(gBrowser.selectedBrowser, [url], urlChild => {
    const script = content.document.createElement("script");
    script.setAttribute("src", urlChild);
    content.document.body.appendChild(script);
  });
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
    const { resourceCommand } = toolbox;

    function onAvailable(sources) {
      for (const source of sources) {
        if (source.url === url) {
          resourceCommand.unwatchResources([resourceCommand.TYPES.SOURCE], {
            onAvailable,
          });
          resolve();
        }
      }
    }
    resourceCommand.watchResources([resourceCommand.TYPES.SOURCE], {
      onAvailable,
    });
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
    return new Promise(resolve => {
      executeSoon(() => {
        resolve(this);
      });
    });
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

  destroy: function() {
    return Promise.resolve(null);
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
  const chevronMenuButton = toolbox.doc.querySelector(".tools-chevron-menu");
  EventUtils.synthesizeMouseAtCenter(chevronMenuButton, {}, toolbox.win);

  const menuPopup = toolbox.doc.getElementById(
    "tools-chevron-menu-button-panel"
  );
  ok(menuPopup, "tools-chevron-menupopup is available");

  info("Waiting for the menu popup to be displayed");
  await waitUntil(() => menuPopup.classList.contains("tooltip-visible"));
}

async function closeChevronMenu(toolbox) {
  // In order to close the popup menu with escape key, set the focus to the chevron
  // button at first.
  const chevronMenuButton = toolbox.doc.querySelector(".tools-chevron-menu");
  chevronMenuButton.focus();

  EventUtils.sendKey("ESCAPE", toolbox.doc.defaultView);
  const menuPopup = toolbox.doc.getElementById(
    "tools-chevron-menu-button-panel"
  );

  info("Closing the chevron popup menu");
  await waitUntil(() => !menuPopup.classList.contains("tooltip-visible"));
}

function prepareToolTabReorderTest(toolbox, startingOrder) {
  Services.prefs.setCharPref(
    "devtools.toolbox.tabsOrder",
    startingOrder.join(",")
  );
  ok(
    !toolbox.doc.getElementById("tools-chevron-menu-button"),
    "The size of the screen being too small"
  );

  for (const id of startingOrder) {
    ok(getElementByToolId(toolbox, id), `Tab element should exist for ${id}`);
  }
}

async function dndToolTab(toolbox, dragTarget, dropTarget, passedTargets = []) {
  info(`Drag ${dragTarget} to ${dropTarget}`);
  const dragTargetEl = getElementByToolIdOrExtensionIdOrSelector(
    toolbox,
    dragTarget
  );

  const onReady = dragTargetEl.classList.contains("selected")
    ? Promise.resolve()
    : toolbox.once("select");
  EventUtils.synthesizeMouseAtCenter(
    dragTargetEl,
    { type: "mousedown" },
    dragTargetEl.ownerGlobal
  );
  await onReady;

  for (const passedTarget of passedTargets) {
    info(`Via ${passedTarget}`);
    const passedTargetEl = getElementByToolIdOrExtensionIdOrSelector(
      toolbox,
      passedTarget
    );
    EventUtils.synthesizeMouseAtCenter(
      passedTargetEl,
      { type: "mousemove" },
      passedTargetEl.ownerGlobal
    );
  }

  if (dropTarget) {
    const dropTargetEl = getElementByToolIdOrExtensionIdOrSelector(
      toolbox,
      dropTarget
    );
    EventUtils.synthesizeMouseAtCenter(
      dropTargetEl,
      { type: "mousemove" },
      dropTargetEl.ownerGlobal
    );
    EventUtils.synthesizeMouseAtCenter(
      dropTargetEl,
      { type: "mouseup" },
      dropTargetEl.ownerGlobal
    );
  } else {
    const containerEl = toolbox.doc.getElementById("toolbox-container");
    EventUtils.synthesizeMouse(
      containerEl,
      0,
      0,
      { type: "mouseout" },
      containerEl.ownerGlobal
    );
  }

  // Wait for updating the preference.
  await new Promise(resolve => {
    const onUpdated = () => {
      Services.prefs.removeObserver("devtools.toolbox.tabsOrder", onUpdated);
      resolve();
    };

    Services.prefs.addObserver("devtools.toolbox.tabsOrder", onUpdated);
  });
}

function assertToolTabOrder(toolbox, expectedOrder) {
  info("Check the order of the tabs on the toolbar");

  const tabEls = toolbox.doc.querySelectorAll(".devtools-tab");

  for (let i = 0; i < expectedOrder.length; i++) {
    const isOrdered =
      tabEls[i].dataset.id === expectedOrder[i] ||
      tabEls[i].dataset.extensionId === expectedOrder[i];
    ok(isOrdered, `The tab[${expectedOrder[i]}] should exist at [${i}]`);
  }
}

function assertToolTabSelected(toolbox, dragTarget) {
  info("Check whether the drag target was selected");
  const dragTargetEl = getElementByToolIdOrExtensionIdOrSelector(
    toolbox,
    dragTarget
  );
  ok(
    dragTargetEl.classList.contains("selected"),
    "The dragged tool should be selected"
  );
}

function assertToolTabPreferenceOrder(expectedOrder) {
  info("Check the order in DevTools preference for tabs order");
  is(
    Services.prefs.getCharPref("devtools.toolbox.tabsOrder"),
    expectedOrder.join(","),
    "The preference should be correct"
  );
}

function getElementByToolId(toolbox, id) {
  for (const tabEl of toolbox.doc.querySelectorAll(".devtools-tab")) {
    if (tabEl.dataset.id === id || tabEl.dataset.extensionId === id) {
      return tabEl;
    }
  }

  return null;
}

function getElementByToolIdOrExtensionIdOrSelector(toolbox, idOrSelector) {
  const tabEl = getElementByToolId(toolbox, idOrSelector);
  return tabEl ? tabEl : toolbox.doc.querySelector(idOrSelector);
}

/**
 * Returns a toolbox tab element, even if it's overflowed
 **/
function getToolboxTab(doc, toolId) {
  return (
    doc.getElementById(`toolbox-tab-${toolId}`) ||
    doc.getElementById(`tools-chevron-menupopup-${toolId}`)
  );
}

function getWindow(toolbox) {
  return toolbox.topWindow;
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

function assertSelectedLocationInDebugger(debuggerPanel, line, column) {
  const location = debuggerPanel._selectors.getSelectedLocation(
    debuggerPanel._getState()
  );
  is(location.line, line);
  is(location.column, column);
}

/**
 * Open a new tab on about:devtools-toolbox with the provided params object used as
 * queryString.
 */
async function openAboutToolbox(params) {
  info("Open about:devtools-toolbox");
  const querystring = new URLSearchParams();
  Object.keys(params).forEach(x => querystring.append(x, params[x]));

  const tab = await addTab(`about:devtools-toolbox?${querystring}`);
  const browser = tab.linkedBrowser;

  return {
    tab,
    document: browser.contentDocument,
  };
}

/**
 * Load FTL.
 *
 * @param {Toolbox} toolbox
 *        Toolbox instance.
 * @param {String} path
 *        Path to the FTL file.
 */
function loadFTL(toolbox, path) {
  const win = toolbox.doc.ownerGlobal;

  if (win.MozXULElement) {
    win.MozXULElement.insertFTLIfNeeded(path);
  }
}

/**
 * Emit a reload key shortcut from a given toolbox, and wait for the reload to
 * be completed.
 *
 * @param {String} shortcut
 *        The key shortcut to send, as expected by the devtools shortcuts
 *        helpers (eg. "CmdOrCtrl+F5").
 * @param {Toolbox} toolbox
 *        The toolbox through which the event should be emitted.
 */
async function sendToolboxReloadShortcut(shortcut, toolbox) {
  const promises = [];

  // If we have a jsdebugger panel, wait for it to complete its reload.
  const jsdebugger = toolbox.getPanel("jsdebugger");
  if (jsdebugger) {
    promises.push(jsdebugger.once("reloaded"));
  }

  // If we have an inspector panel, wait for it to complete its reload.
  const inspector = toolbox.getPanel("inspector");
  if (inspector) {
    promises.push(
      inspector.once("reloaded"),
      inspector.once("inspector-updated")
    );
  }

  const loadPromise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  promises.push(loadPromise);

  info("Focus the toolbox window and emit the reload shortcut: " + shortcut);
  toolbox.win.focus();
  synthesizeKeyShortcut(shortcut, toolbox.win);

  info("Wait for page and toolbox reload promises");
  await Promise.all(promises);
}

function getErrorIcon(toolbox) {
  return toolbox.doc.querySelector(".toolbox-error");
}

function getErrorIconCount(toolbox) {
  const textContent = getErrorIcon(toolbox)?.textContent;
  try {
    const int = parseInt(textContent, 10);
    // 99+ parses to 99, so we check if the parsedInt does not match the textContent.
    return int.toString() === textContent ? int : textContent;
  } catch (e) {
    // In case the parseInt threw, return the actual textContent so the test can display
    // an easy to debug failure.
    return textContent;
  }
}
