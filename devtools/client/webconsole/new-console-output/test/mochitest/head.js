/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from ../../../../framework/test/shared-head.js */
/* eslint no-unused-vars: [2, {"vars": "local"}] */

"use strict";

// Import helpers registering the test-actor in remote targets
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/shared/test/test-actor-registry.js",
  this);

// shared-head.js handles imports, constants, and utility functions
// Load the shared-head file first.
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/framework/test/shared-head.js",
  this);

var {HUDService} = require("devtools/client/webconsole/hudservice");
var WCUL10n = require("devtools/client/webconsole/webconsole-l10n");
const DOCS_GA_PARAMS = `?${new URLSearchParams({
  "utm_source": "mozilla",
  "utm_medium": "firefox-console-errors",
  "utm_campaign": "default"
})}`;
const STATUS_CODES_GA_PARAMS = `?${new URLSearchParams({
  "utm_source": "mozilla",
  "utm_medium": "devtools-webconsole",
  "utm_campaign": "default"
})}`;

Services.prefs.setBoolPref("devtools.webconsole.new-frontend-enabled", true);
registerCleanupFunction(function* () {
  Services.prefs.clearUserPref("devtools.webconsole.new-frontend-enabled");
  Services.prefs.clearUserPref("devtools.webconsole.ui.filterbar");

  // Reset all filter prefs between tests. First flushPrefEnv in case one of the
  // filter prefs has been pushed for the test
  yield SpecialPowers.flushPrefEnv();
  Services.prefs.getChildList("devtools.webconsole.filter").forEach(pref => {
    Services.prefs.clearUserPref(pref);
  });
  let browserConsole = HUDService.getBrowserConsole();
  if (browserConsole) {
    if (browserConsole.jsterm) {
      browserConsole.jsterm.clearOutput(true);
    }
    yield HUDService.toggleBrowserConsole();
  }
});

/**
 * Add a new tab and open the toolbox in it, and select the webconsole.
 *
 * @param string url
 *        The URL for the tab to be opened.
 * @param Boolean clearJstermHistory
 *        true (default) if the jsterm history should be cleared.
 * @return Promise
 *         Resolves when the tab has been added, loaded and the toolbox has been opened.
 *         Resolves to the toolbox.
 */
async function openNewTabAndConsole(url, clearJstermHistory = true) {
  let toolbox = await openNewTabAndToolbox(url, "webconsole");
  let hud = toolbox.getCurrentPanel().hud;
  hud.jsterm._lazyVariablesView = false;

  if (clearJstermHistory) {
    // Clearing history that might have been set in previous tests.
    await hud.jsterm.clearHistory();
  }

  return hud;
}

/**
 * Subscribe to the store and log out stringinfied versions of messages.
 * This is a helper function for debugging, to make is easier to see what
 * happened during the test in the log.
 *
 * @param object hud
 */
function logAllStoreChanges(hud) {
  const store = hud.ui.newConsoleOutput.getStore();
  // Adding logging each time the store is modified in order to check
  // the store state in case of failure.
  store.subscribe(() => {
    const messages = [...store.getState().messages.messagesById.values()];
    const debugMessages = messages.map(({id, type, parameters, messageText}) => {
      return {id, type, parameters, messageText};
    });
    info("messages : " + JSON.stringify(debugMessages));
  });
}

/**
 * Wait for messages in the web console output, resolving once they are received.
 *
 * @param object options
 *        - hud: the webconsole
 *        - messages: Array[Object]. An array of messages to match.
            Current supported options:
 *            - text: Partial text match in .message-body
 */
function waitForMessages({ hud, messages }) {
  return new Promise(resolve => {
    const matchedMessages = [];
    hud.ui.on("new-messages",
      function messagesReceived(e, newMessages) {
        for (let message of messages) {
          if (message.matched) {
            continue;
          }

          for (let newMessage of newMessages) {
            let messageBody = newMessage.node.querySelector(".message-body");
            if (messageBody.textContent.includes(message.text)) {
              matchedMessages.push(newMessage);
              message.matched = true;
              const messagesLeft = messages.length - matchedMessages.length;
              info(`Matched a message with text: "${message.text}", ` + (messagesLeft > 0
                ? `still waiting for ${messagesLeft} messages.`
                : `all messages received.`)
              );
              break;
            }
          }

          if (matchedMessages.length === messages.length) {
            hud.ui.off("new-messages", messagesReceived);
            resolve(matchedMessages);
            return;
          }
        }
      });
  });
}

/**
 * Wait for a single message in the web console output, resolving once it is received.
 *
 * @param {Object} hud : the webconsole
 * @param {String} text : text included in .message-body
 */
async function waitForMessage(hud, text) {
  const messages = await waitForMessages({hud, messages: [{text}]});
  return messages[0];
}

/**
 * Wait for a predicate to return a result.
 *
 * @param function condition
 *        Invoked once in a while until it returns a truthy value. This should be an
 *        idempotent function, since we have to run it a second time after it returns
 *        true in order to return the value.
 * @param string message [optional]
 *        A message to output if the condition fails.
 * @param number interval [optional]
 *        How often the predicate is invoked, in milliseconds.
 * @return object
 *         A promise that is resolved with the result of the condition.
 */
async function waitFor(condition, message = "waitFor", interval = 10, maxTries = 500) {
  await BrowserTestUtils.waitForCondition(condition, message, interval, maxTries);
  return condition();
}

/**
 * Find a message in the output.
 *
 * @param object hud
 *        The web console.
 * @param string text
 *        A substring that can be found in the message.
 * @param selector [optional]
 *        The selector to use in finding the message.
 */
function findMessage(hud, text, selector = ".message") {
  const elements = findMessages(hud, text, selector);
  return elements.pop();
}

/**
 * Find multiple messages in the output.
 *
 * @param object hud
 *        The web console.
 * @param string text
 *        A substring that can be found in the message.
 * @param selector [optional]
 *        The selector to use in finding the message.
 */
function findMessages(hud, text, selector = ".message") {
  const messages = hud.ui.outputNode.querySelectorAll(selector);
  const elements = Array.prototype.filter.call(
    messages,
    (el) => el.textContent.includes(text)
  );
  return elements;
}

/**
 * Simulate a context menu event on the provided element, and wait for the console context
 * menu to open. Returns a promise that resolves the menu popup element.
 *
 * @param object hud
 *        The web console.
 * @param element element
 *        The dom element on which the context menu event should be synthesized.
 * @return promise
 */
async function openContextMenu(hud, element) {
  let onConsoleMenuOpened = hud.ui.newConsoleOutput.once("menu-open");
  synthesizeContextMenuEvent(element);
  await onConsoleMenuOpened;
  return hud.ui.newConsoleOutput.toolbox.doc.getElementById("webconsole-menu");
}

/**
 * Hide the webconsole context menu popup. Returns a promise that will resolve when the
 * context menu popup is hidden or immediately if the popup can't be found.
 *
 * @param object hud
 *        The web console.
 * @return promise
 */
function hideContextMenu(hud) {
  let popup = hud.ui.newConsoleOutput.toolbox.doc.getElementById("webconsole-menu");
  if (!popup) {
    return Promise.resolve();
  }

  let onPopupHidden = once(popup, "popuphidden");
  popup.hidePopup();
  return onPopupHidden;
}

function loadDocument(url, browser = gBrowser.selectedBrowser) {
  BrowserTestUtils.loadURI(browser, url);
  return BrowserTestUtils.browserLoaded(browser);
}

/**
* Returns a promise that resolves when the node passed as an argument mutate
* according to the passed configuration.
*
* @param {Node} node - The node to observe mutations on.
* @param {Object} observeConfig - A configuration object for MutationObserver.observe.
* @returns {Promise}
*/
function waitForNodeMutation(node, observeConfig = {}) {
  return new Promise(resolve => {
    const observer = new MutationObserver(mutations => {
      resolve(mutations);
      observer.disconnect();
    });
    observer.observe(node, observeConfig);
  });
}

/**
 * Search for a given message.  When found, simulate a click on the
 * message's location, checking to make sure that the debugger opens
 * the corresponding URL.
 *
 * @param {Object} hud
 *        The webconsole
 * @param {Object} toolbox
 *        The toolbox
 * @param {String} text
 *        The text to search for.  This should be contained in the
 *        message.  The searching is done with @see findMessage.
 */
async function testOpenInDebugger(hud, toolbox, text) {
  info(`Finding message for open-in-debugger test; text is "${text}"`);
  let messageNode = await waitFor(() => findMessage(hud, text));
  let frameLinkNode = messageNode.querySelector(".message-location .frame-link");
  ok(frameLinkNode, "The message does have a location link");
  await checkClickOnNode(hud, toolbox, frameLinkNode);
}

/**
 * Helper function for testOpenInDebugger.
 */
async function checkClickOnNode(hud, toolbox, frameLinkNode) {
  info("checking click on node location");

  let url = frameLinkNode.getAttribute("data-url");
  ok(url, `source url found ("${url}")`);

  let line = frameLinkNode.getAttribute("data-line");
  ok(line, `source line found ("${line}")`);

  let onSourceInDebuggerOpened = once(hud.ui, "source-in-debugger-opened");

  EventUtils.sendMouseEvent({ type: "click" },
    frameLinkNode.querySelector(".frame-link-filename"));

  await onSourceInDebuggerOpened;

  let dbg = toolbox.getPanel("jsdebugger");
  is(
    dbg._selectors.getSelectedSource(dbg._getState()).get("url"),
    url,
    "expected source url"
  );
}

/**
 * Returns true if the give node is currently focused.
 */
function hasFocus(node) {
  return node.ownerDocument.activeElement == node
    && node.ownerDocument.hasFocus();
}

/**
 * Set the value of the JsTerm and its caret position, and fire a completion request.
 *
 * @param {JsTerm} jsterm
 * @param {String} value : The value to set the jsterm to.
 * @param {Integer} caretIndexOffset : A number that will be added to value.length
 *                  when setting the caret. A negative number will place the caret
 *                  in (end - offset) position. Default to 0 (caret set at the end)
 * @param {Integer} completionType : One of the following jsterm property
 *                   - COMPLETE_FORWARD
 *                   - COMPLETE_BACKWARD
 *                   - COMPLETE_HINT_ONLY
 *                   - COMPLETE_PAGEUP
 *                   - COMPLETE_PAGEDOWN
 *                  Will default to COMPLETE_HINT_ONLY.
 * @returns {Promise} resolves when the jsterm is completed.
 */
function jstermSetValueAndComplete(jsterm, value, caretIndexOffset = 0, completionType) {
  const {inputNode} = jsterm;
  inputNode.value = value;
  let index = value.length + caretIndexOffset;
  inputNode.setSelectionRange(index, index);

  return jstermComplete(jsterm, completionType);
}

/**
 * Fires a completion request on the jsterm with the specified completionType
 *
 * @param {JsTerm} jsterm
 * @param {Integer} completionType : One of the following jsterm property
 *                   - COMPLETE_FORWARD
 *                   - COMPLETE_BACKWARD
 *                   - COMPLETE_HINT_ONLY
 *                   - COMPLETE_PAGEUP
 *                   - COMPLETE_PAGEDOWN
 *                  Will default to COMPLETE_HINT_ONLY.
 * @returns {Promise} resolves when the jsterm is completed.
 */
function jstermComplete(jsterm, completionType = jsterm.COMPLETE_HINT_ONLY) {
  const updated = jsterm.once("autocomplete-updated");
  jsterm.complete(completionType);
  return updated;
}

/**
 * Open the JavaScript debugger.
 *
 * @param object options
 *        Options for opening the debugger:
 *        - tab: the tab you want to open the debugger for.
 * @return object
 *         A promise that is resolved once the debugger opens, or rejected if
 *         the open fails. The resolution callback is given one argument, an
 *         object that holds the following properties:
 *         - target: the Target object for the Tab.
 *         - toolbox: the Toolbox instance.
 *         - panel: the jsdebugger panel instance.
 */
async function openDebugger(options = {}) {
  if (!options.tab) {
    options.tab = gBrowser.selectedTab;
  }

  let target = TargetFactory.forTab(options.tab);
  let toolbox = gDevTools.getToolbox(target);
  let dbgPanelAlreadyOpen = toolbox && toolbox.getPanel("jsdebugger");
  if (dbgPanelAlreadyOpen) {
    await toolbox.selectTool("jsdebugger");

    return {
      target,
      toolbox,
      panel: toolbox.getCurrentPanel()
    };
  }

  toolbox = await gDevTools.showToolbox(target, "jsdebugger");
  let panel = toolbox.getCurrentPanel();

  // Do not clear VariableView lazily so it doesn't disturb test ending.
  panel._view.Variables.lazyEmpty = false;

  await panel.panelWin.DebuggerController.waitForSourcesLoaded();
  return {target, toolbox, panel};
}

/**
 * Open the Web Console for the given tab, or the current one if none given.
 *
 * @param nsIDOMElement tab
 *        Optional tab element for which you want open the Web Console.
 *        Defaults to current selected tab.
 * @return Promise
 *         A promise that is resolved with the console hud once the web console is open.
 */
async function openConsole(tab) {
  let target = TargetFactory.forTab(tab || gBrowser.selectedTab);
  const toolbox = await gDevTools.showToolbox(target, "webconsole");
  return toolbox.getCurrentPanel().hud;
}

/**
 * Close the Web Console for the given tab.
 *
 * @param nsIDOMElement [tab]
 *        Optional tab element for which you want close the Web Console.
 *        Defaults to current selected tab.
 * @return object
 *         A promise that is resolved once the web console is closed.
 */
async function closeConsole(tab = gBrowser.selectedTab) {
  let target = TargetFactory.forTab(tab);
  let toolbox = gDevTools.getToolbox(target);
  if (toolbox) {
    await toolbox.destroy();
  }
}

/**
 * Fake clicking a link and return the URL we would have navigated to.
 * This function should be used to check external links since we can't access
 * network in tests.
 * This can also be used to test that a click will not be fired.
 *
 * @param ElementNode element
 *        The <a> element we want to simulate click on.
 * @param Object clickEventProps
 *        The custom properties which would be used to dispatch a click event
 * @returns Promise
 *          A Promise that is resolved when the link click simulation occured or when the click is not dispatched.
 *          The promise resolves with an object that holds the following properties
 *          - link: url of the link or null(if event not fired)
 *          - where: "tab" if tab is active or "tabshifted" if tab is inactive or null(if event not fired)
 */
function simulateLinkClick(element, clickEventProps) {
  // Override openUILinkIn to prevent navigating.
  let oldOpenUILinkIn = window.openUILinkIn;

  const onOpenLink = new Promise((resolve) => {
    window.openUILinkIn = function (link, where) {
      window.openUILinkIn = oldOpenUILinkIn;
      resolve({link: link, where});
    };

    if (clickEventProps) {
      // Click on the link using the event properties.
      element.dispatchEvent(clickEventProps);
    } else {
      // Click on the link.
      element.click();
    }
  });

  // Declare a timeout Promise that we can use to make sure openUILinkIn was not called.
  let timeoutId;
  const onTimeout = new Promise(function(resolve, reject) {
    timeoutId = setTimeout(() => {
      window.openUILinkIn = oldOpenUILinkIn;
      timeoutId = null;
      resolve({link: null, where: null});
    }, 1000);
  });

  onOpenLink.then(() => {
    if (timeoutId) {
      clearTimeout(timeoutId);
    }
  });
  return Promise.race([onOpenLink, onTimeout]);
}

/**
 * Open a new browser window and return a promise that resolves when the new window has
 * fired the "browser-delayed-startup-finished" event.
 *
 * @returns Promise
 *          A Promise that resolves when the window is ready.
 */
function openNewBrowserWindow() {
  let win = OpenBrowserWindow();
  return new Promise(resolve => {
    Services.obs.addObserver(function observer(subject, topic) {
      if (win == subject) {
        Services.obs.removeObserver(observer, topic);
        resolve(win);
      }
    }, "browser-delayed-startup-finished");
  });
}

/**
 * Open a network request logged in the webconsole in the netmonitor panel.
 *
 * @param {Object} toolbox
 * @param {Object} hud
 * @param {String} url
 *        URL of the request as logged in the netmonitor.
 * @param {String} urlInConsole
 *        (optional) Use if the logged URL in webconsole is different from the real URL.
 */
async function openMessageInNetmonitor(toolbox, hud, url, urlInConsole) {
  // By default urlInConsole should be the same as the complete url.
  urlInConsole = urlInConsole || url;

  let message = await waitFor(() => findMessage(hud, urlInConsole));

  let onNetmonitorSelected = toolbox.once("netmonitor-selected", (event, panel) => {
    return panel;
  });

  let menuPopup = await openContextMenu(hud, message);
  let openInNetMenuItem = menuPopup.querySelector("#console-menu-open-in-network-panel");
  ok(openInNetMenuItem, "open in network panel item is enabled");
  openInNetMenuItem.click();

  const {panelWin} = await onNetmonitorSelected;
  ok(true, "The netmonitor panel is selected when clicking on the network message");

  let { store, windowRequire } = panelWin;
  let actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  let { getSelectedRequest } =
    windowRequire("devtools/client/netmonitor/src/selectors/index");

  store.dispatch(actions.batchEnable(false));

  await waitUntil(() => {
    const selected = getSelectedRequest(store.getState());
    return selected && selected.url === url;
  });

  ok(true, "The attached url is correct.");
}
