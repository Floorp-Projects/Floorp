/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint no-unused-vars: [2, {"vars": "local"}] */
/* import-globals-from ../../../shared/test/telemetry-test-helpers.js */

"use strict";

// Import helpers registering the test-actor in remote targets
/* globals registerTestActor, getTestActor, Task, openToolboxForTab, gBrowser */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/shared/test/test-actor-registry.js",
  this);

// shared-head.js handles imports, constants, and utility functions
// Load the shared-head file first.
/* import-globals-from ../../../shared/test/shared-head.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/shared/test/shared-head.js",
  this);

// Import helpers for the new debugger
/* import-globals-from ../../../debugger/test/mochitest/helpers/context.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/debugger/test/mochitest/helpers/context.js",
  this);

// Import helpers for the new debugger
/* import-globals-from ../../../debugger/test/mochitest/helpers.js*/
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/debugger/test/mochitest/helpers.js",
  this);

var {HUDService} = require("devtools/client/webconsole/hudservice");
var WCUL10n = require("devtools/client/webconsole/webconsole-l10n");
const DOCS_GA_PARAMS = `?${new URLSearchParams({
  "utm_source": "mozilla",
  "utm_medium": "firefox-console-errors",
  "utm_campaign": "default",
})}`;
const GA_PARAMS = `?${new URLSearchParams({
  "utm_source": "mozilla",
  "utm_medium": "devtools-webconsole",
  "utm_campaign": "default",
})}`;

const wcActions = require("devtools/client/webconsole/actions/index");

registerCleanupFunction(async function() {
  Services.prefs.clearUserPref("devtools.webconsole.ui.filterbar");

  // Reset all filter prefs between tests. First flushPrefEnv in case one of the
  // filter prefs has been pushed for the test
  await SpecialPowers.flushPrefEnv();
  Services.prefs.getChildList("devtools.webconsole.filter").forEach(pref => {
    Services.prefs.clearUserPref(pref);
  });
  const browserConsole = HUDService.getBrowserConsole();
  if (browserConsole) {
    browserConsole.ui.clearOutput(true);
    await HUDService.toggleBrowserConsole();
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
  const toolbox = await openNewTabAndToolbox(url, "webconsole");
  const hud = toolbox.getCurrentPanel().hud;

  if (clearJstermHistory) {
    // Clearing history that might have been set in previous tests.
    await hud.ui.wrapper.dispatchClearHistory();
  }

  return hud;
}

/**
 * Open a new window with a tab,open the toolbox, and select the webconsole.
 *
 * @param string url
 *        The URL for the tab to be opened.
 * @return Promise<{win, hud, tab}>
 *         Resolves when the tab has been added, loaded and the toolbox has been opened.
 *         Resolves to the toolbox.
 */
async function openNewWindowAndConsole(url) {
  const win = await openNewBrowserWindow();
  const tab = await addTab(url, {window: win});
  win.gBrowser.selectedTab = tab;
  const hud = await openConsole(tab);
  return {win, hud, tab};
}

/**
 * Subscribe to the store and log out stringinfied versions of messages.
 * This is a helper function for debugging, to make is easier to see what
 * happened during the test in the log.
 *
 * @param object hud
 */
function logAllStoreChanges(hud) {
  const store = hud.ui.wrapper.getStore();
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
 *        - selector: {String} a selector that should match the message node. Defaults to
 *                             ".message".
 */
function waitForMessages({ hud, messages, selector = ".message" }) {
  return new Promise(resolve => {
    const matchedMessages = [];
    hud.ui.on("new-messages",
      function messagesReceived(newMessages) {
        for (const message of messages) {
          if (message.matched) {
            continue;
          }

          for (const newMessage of newMessages) {
            const messageBody =
              newMessage.node.querySelector(`.message-body`);
            if (
              messageBody &&
              newMessage.node.matches(selector) &&
              messageBody.textContent.includes(message.text)
            ) {
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
 * Wait for a message with the provided text and showing the provided repeat count.
 *
 * @param {Object} hud : the webconsole
 * @param {String} text : text included in .message-body
 * @param {Number} repeat : expected repeat count in .message-repeats
 */
function waitForRepeatedMessage(hud, text, repeat) {
  return waitFor(() => {
    // Wait for a message matching the provided text.
    const node = findMessage(hud, text);
    if (!node) {
      return false;
    }

    // Check if there is a repeat node with the expected count.
    const repeatNode = node.querySelector(".message-repeats");
    if (repeatNode && parseInt(repeatNode.textContent, 10) === repeat) {
      return node;
    }

    return false;
  });
}

/**
 * Wait for a single message in the web console output, resolving once it is received.
 *
 * @param {Object} hud : the webconsole
 * @param {String} text : text included in .message-body
 * @param {String} selector : A selector that should match the message node.
 */
async function waitForMessage(hud, text, selector) {
  const messages = await waitForMessages({
    hud,
    messages: [{text}],
    selector,
  });
  return messages[0];
}

/**
 * Execute an input expression and wait for a message with the expected text (and an
 * optional selector) to be displayed in the output.
 *
 * @param {Object} hud : The webconsole.
 * @param {String} input : The input expression to execute.
 * @param {String} matchingText : A string that should match the message body content.
 * @param {String} selector : A selector that should match the message node.
 */
function executeAndWaitForMessage(hud, input, matchingText, selector = ".message") {
  const onMessage = waitForMessage(hud, matchingText, selector);
  hud.jsterm.execute(input);
  return onMessage;
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
 * @return {Node} the node corresponding the found message
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
  const onConsoleMenuOpened = hud.ui.wrapper.once("menu-open");
  synthesizeContextMenuEvent(element);
  await onConsoleMenuOpened;
  return _getContextMenu(hud);
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
  const popup = _getContextMenu(hud);
  if (!popup) {
    return Promise.resolve();
  }

  const onPopupHidden = once(popup, "popuphidden");
  popup.hidePopup();
  return onPopupHidden;
}

function _getContextMenu(hud) {
  const toolbox = gDevTools.getToolbox(hud.target);
  const doc = toolbox ? toolbox.topWindow.document : hud.chromeWindow.document;
  return doc.getElementById("webconsole-menu");
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
 * @param {boolean} expectUrl
 *        Whether the URL in the opened source should match the link, or whether
 *        it is expected to be null.
 * @param {boolean} expectLine
 *        It indicates if there is the need to check the line.
 * @param {boolean} expectColumn
 *        It indicates if there is the need to check the column.
 */
async function testOpenInDebugger(
  hud,
  toolbox,
  text,
  expectUrl = true,
  expectLine = true,
  expectColumn = true
) {
  info(`Finding message for open-in-debugger test; text is "${text}"`);
  const messageNode = await waitFor(() => findMessage(hud, text));
  const frameLinkNode = messageNode.querySelector(".message-location .frame-link");
  ok(frameLinkNode, "The message does have a location link");
  await checkClickOnNode(hud,
                         toolbox,
                         frameLinkNode,
                         expectUrl,
                         expectLine,
                         expectColumn);
}

/**
 * Helper function for testOpenInDebugger.
 */
async function checkClickOnNode(
  hud,
  toolbox,
  frameLinkNode,
  expectUrl,
  expectLine,
  expectColumn
) {
  info("checking click on node location");

  const onSourceInDebuggerOpened = once(hud.ui, "source-in-debugger-opened");

  EventUtils.sendMouseEvent({ type: "click" },
    frameLinkNode.querySelector(".frame-link-filename"));

  await onSourceInDebuggerOpened;

  const dbg = toolbox.getPanel("jsdebugger");

  // Wait for the source to finish loading, if it is pending.
  await waitFor(() => !!dbg._selectors.getSelectedSource(dbg._getState()) &&
                      !!dbg._selectors.getSelectedLocation(dbg._getState()));

  if (expectUrl) {
    const url = frameLinkNode.getAttribute("data-url");
    ok(url, `source url found ("${url}")`);

    is(
      dbg._selectors.getSelectedSource(dbg._getState()).url, url,
      "expected source url"
    );
  }
  if (expectLine) {
    const line = frameLinkNode.getAttribute("data-line");
    ok(line, `source line found ("${line}")`);

    is(
      dbg._selectors.getSelectedLocation(dbg._getState()).line, line,
      "expected source line"
    );
  }
  if (expectColumn) {
    const column = frameLinkNode.getAttribute("data-column");
    ok(column, `source column found ("${column}")`);

    is(
      dbg._selectors.getSelectedLocation(dbg._getState()).column, column,
      "expected source column"
    );
  }
}

/**
 * Returns true if the give node is currently focused.
 */
function hasFocus(node) {
  return node.ownerDocument.activeElement == node
    && node.ownerDocument.hasFocus();
}

/**
 * Get the value of the console input .
 *
 * @param {WebConsole} hud: The webconsole
 * @returns {String}: The value of the console input.
 */
function getInputValue(hud) {
  return hud.jsterm._getValue();
}

/**
 * Set the value of the console input .
 *
 * @param {WebConsole} hud: The webconsole
 * @param {String} value : The value to set the console input to.
 */
function setInputValue(hud, value) {
  return hud.jsterm._setValue(value);
}

/**
 * Set the value of the console input and its caret position, and wait for the
 * autocompletion to be updated.
 *
 * @param {WebConsole} hud: The webconsole
 * @param {String} value : The value to set the jsterm to.
 * @param {Integer} caretPosition : The index where to place the cursor. A negative
 *                  number will place the caret at (value.length - offset) position.
 *                  Default to value.length (caret set at the end).
 * @returns {Promise} resolves when the jsterm is completed.
 */
async function setInputValueForAutocompletion(
  hud,
  value,
  caretPosition = value.length,
) {
  const {jsterm} = hud;

  setInputValue(hud, "");
  jsterm.focus();

  const updated = jsterm.once("autocomplete-updated");
  EventUtils.sendString(value);
  await updated;

  if (caretPosition < 0) {
    caretPosition = value.length + caretPosition;
  }

  if (Number.isInteger(caretPosition)) {
    if (jsterm.inputNode) {
      const {inputNode} = jsterm;
      inputNode.value = value;
      inputNode.setSelectionRange(caretPosition, caretPosition);
    }

    if (jsterm.editor) {
      jsterm.editor.setCursor(jsterm.editor.getPosition(caretPosition));
    }
  }
}

/**
 * Set the value of the console input and wait for the confirm dialog to be displayed.
 *
 * @param {Toolbox} toolbox
 * @param {WebConsole} hud
 * @param {String} value : The value to set the jsterm to.
 *                  Default to value.length (caret set at the end).
 * @returns {Promise<HTMLElement>} resolves with dialog element when it is opened.
 */
async function setInputValueForGetterConfirmDialog(toolbox, hud, value) {
  await setInputValueForAutocompletion(hud, value);
  await waitFor(() => isConfirmDialogOpened(toolbox));
  ok(true, "The confirm dialog is displayed");
  return getConfirmDialog(toolbox);
}

/**
 * Checks if the console input has the expected completion value.
 *
 * @param {WebConsole} hud
 * @param {String} expectedValue
 * @param {String} assertionInfo: Description of the assertion passed to `is`.
 */
function checkInputCompletionValue(hud, expectedValue, assertionInfo) {
  const completionValue = getInputCompletionValue(hud);
  if (completionValue === null) {
    ok(false, "Couldn't retrieve the completion value");
  }

  info(`Expects "${expectedValue}", is "${completionValue}"`);

  if (hud.jsterm.completeNode) {
    is(completionValue, expectedValue, assertionInfo);
  } else {
    // CodeMirror jsterm doesn't need to add prefix-spaces.
    is(completionValue, expectedValue.trim(), assertionInfo);
  }
}

/**
 * Checks if the cursor on console input is at expected position.
 *
 * @param {WebConsole} hud
 * @param {Integer} expectedCursorIndex
 * @param {String} assertionInfo: Description of the assertion passed to `is`.
 */
function checkInputCursorPosition(hud, expectedCursorIndex, assertionInfo) {
  const {jsterm} = hud;
  if (jsterm.inputNode) {
    const {selectionStart, selectionEnd} = jsterm.inputNode;
    is(selectionStart, expectedCursorIndex, assertionInfo);
    ok(selectionStart === selectionEnd);
  } else {
    is(jsterm.editor.getCursor().ch, expectedCursorIndex, assertionInfo);
  }
}

/**
 * Checks the console input value and the cursor position given an expected string
 * containing a "|" to indicate the expected cursor position.
 *
 * @param {WebConsole} hud
 * @param {String} expectedStringWithCursor:
 *                  String with a "|" to indicate the expected cursor position.
 *                  For example, this is how you assert an empty value with the focus "|",
 *                  and this indicates the value should be "test" and the cursor at the
 *                  end of the input: "test|".
 * @param {String} assertionInfo: Description of the assertion passed to `is`.
 */
function checkInputValueAndCursorPosition(hud, expectedStringWithCursor, assertionInfo) {
  info(`Checking jsterm state: \n${expectedStringWithCursor}`);
  if (!expectedStringWithCursor.includes("|")) {
    ok(false,
      `expectedStringWithCursor must contain a "|" char to indicate cursor position`);
  }

  const inputValue = expectedStringWithCursor.replace("|", "");
  const {jsterm} = hud;
  is(getInputValue(hud), inputValue, "console input has expected value");
  if (jsterm.inputNode) {
    is(jsterm.inputNode.selectionStart, jsterm.inputNode.selectionEnd);
    is(jsterm.inputNode.selectionStart, expectedStringWithCursor.indexOf("|"),
      assertionInfo);
  } else {
    const lines = expectedStringWithCursor.split("\n");
    const lineWithCursor = lines.findIndex(line => line.includes("|"));
    const {ch, line} = jsterm.editor.getCursor();
    is(line, lineWithCursor, assertionInfo + " - correct line");
    is(ch, lines[lineWithCursor].indexOf("|"), assertionInfo + " - correct ch");
  }
}

/**
 * Returns the console input completion value.
 *
 * @param {WebConsole} hud
 * @returns {String}
 */
function getInputCompletionValue(hud) {
  const {jsterm} = hud;
  if (jsterm.completeNode) {
    return jsterm.completeNode.value;
  }

  if (jsterm.editor) {
    return jsterm.editor.getAutoCompletionText();
  }

  return null;
}

/**
 * Returns a boolean indicating if the console input is focused.
 *
 * @param {WebConsole} hud
 * @returns {Boolean}
 */
function isInputFocused(hud) {
  const {jsterm} = hud;
  const document = jsterm.outputNode.ownerDocument;
  const documentIsFocused = document.hasFocus();

  if (jsterm.inputNode) {
    return document.activeElement == jsterm.inputNode && documentIsFocused;
  }

  if (jsterm.editor) {
    return documentIsFocused && jsterm.editor.hasFocus();
  }

  return false;
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

  const target = await TargetFactory.forTab(options.tab);
  let toolbox = gDevTools.getToolbox(target);
  const dbgPanelAlreadyOpen = toolbox && toolbox.getPanel("jsdebugger");
  if (dbgPanelAlreadyOpen) {
    await toolbox.selectTool("jsdebugger");

    return {
      target,
      toolbox,
      panel: toolbox.getCurrentPanel(),
    };
  }

  toolbox = await gDevTools.showToolbox(target, "jsdebugger");
  const panel = toolbox.getCurrentPanel();

  // Do not clear VariableView lazily so it doesn't disturb test ending.
  if (panel._view) {
    panel._view.Variables.lazyEmpty = false;
  }

  // Old debugger
  if (panel.panelWin && panel.panelWin.DebuggerController) {
    await panel.panelWin.DebuggerController.waitForSourcesLoaded();
  } else {
    // New debugger
    await toolbox.threadClient.getSources();
  }
  return {target, toolbox, panel};
}

async function openInspector(options = {}) {
  if (!options.tab) {
    options.tab = gBrowser.selectedTab;
  }

  const target = await TargetFactory.forTab(options.tab);
  const toolbox = await gDevTools.showToolbox(target, "inspector");

  return toolbox.getCurrentPanel();
}

/**
 * Open the Web Console for the given tab, or the current one if none given.
 *
 * @param Element tab
 *        Optional tab element for which you want open the Web Console.
 *        Defaults to current selected tab.
 * @return Promise
 *         A promise that is resolved with the console hud once the web console is open.
 */
async function openConsole(tab) {
  const target = await TargetFactory.forTab(tab || gBrowser.selectedTab);
  const toolbox = await gDevTools.showToolbox(target, "webconsole");
  return toolbox.getCurrentPanel().hud;
}

/**
 * Close the Web Console for the given tab.
 *
 * @param Element [tab]
 *        Optional tab element for which you want close the Web Console.
 *        Defaults to current selected tab.
 * @return object
 *         A promise that is resolved once the web console is closed.
 */
async function closeConsole(tab = gBrowser.selectedTab) {
  const target = await TargetFactory.forTab(tab);
  const toolbox = gDevTools.getToolbox(target);
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
 *          A Promise that is resolved when the link click simulation occured or
 *          when the click is not dispatched.
 *          The promise resolves with an object that holds the following properties
 *          - link: url of the link or null(if event not fired)
 *          - where: "tab" if tab is active or "tabshifted" if tab is inactive
 *            or null(if event not fired)
 */
function simulateLinkClick(element, clickEventProps) {
  return overrideOpenLink(() => {
    if (clickEventProps) {
      // Click on the link using the event properties.
      element.dispatchEvent(clickEventProps);
    } else {
      // Click on the link.
      element.click();
    }
  });
}

/**
 * Override the browserWindow open*Link function, executes the passed function and either
 * wait for:
 * - the link to be "opened"
 * - 1s before timing out
 * Then it puts back the original open*Link functions in browserWindow.
 *
 * @returns {Promise<Object>}: A promise resolving with an object of the following shape:
 * - link: The link that was "opened"
 * - where: If the link was opened in the background (null) or not ("tab").
 */
function overrideOpenLink(fn) {
  const browserWindow = Services.wm.getMostRecentWindow(gDevTools.chromeWindowType);

  // Override LinkIn methods to prevent navigating.
  const oldOpenTrustedLinkIn = browserWindow.openTrustedLinkIn;
  const oldOpenWebLinkIn = browserWindow.openWebLinkIn;

  const onOpenLink = new Promise((resolve) => {
    const openLinkIn = function(link, where) {
      browserWindow.openTrustedLinkIn = oldOpenTrustedLinkIn;
      browserWindow.openWebLinkIn = oldOpenWebLinkIn;
      resolve({link: link, where});
    };
    browserWindow.openWebLinkIn = browserWindow.openTrustedLinkIn = openLinkIn;
    fn();
  });

  // Declare a timeout Promise that we can use to make sure openTrustedLinkIn or
  // openWebLinkIn was not called.
  let timeoutId;
  const onTimeout = new Promise(function(resolve) {
    timeoutId = setTimeout(() => {
      browserWindow.openTrustedLinkIn = oldOpenTrustedLinkIn;
      browserWindow.openWebLinkIn = oldOpenWebLinkIn;
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
 * @param {Object} options: An options object that will be passed to OpenBrowserWindow.
 * @returns Promise
 *          A Promise that resolves when the window is ready.
 */
function openNewBrowserWindow(options) {
  const win = OpenBrowserWindow(options);
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

  const message = await waitFor(() => findMessage(hud, urlInConsole));

  const onNetmonitorSelected = toolbox.once("netmonitor-selected", (event, panel) => {
    return panel;
  });

  const menuPopup = await openContextMenu(hud, message);
  const openInNetMenuItem =
    menuPopup.querySelector("#console-menu-open-in-network-panel");
  ok(openInNetMenuItem, "open in network panel item is enabled");
  openInNetMenuItem.click();

  const {panelWin} = await onNetmonitorSelected;
  ok(true, "The netmonitor panel is selected when clicking on the network message");

  const { store, windowRequire } = panelWin;
  const nmActions = windowRequire("devtools/client/netmonitor/src/actions/index");
  const { getSelectedRequest } =
    windowRequire("devtools/client/netmonitor/src/selectors/index");

  store.dispatch(nmActions.batchEnable(false));

  await waitUntil(() => {
    const selected = getSelectedRequest(store.getState());
    return selected && selected.url === url;
  });

  ok(true, "The attached url is correct.");
}

function selectNode(hud, node) {
  const outputContainer = hud.ui.outputNode.querySelector(".webconsole-output");

  // We must first blur the input or else we can't select anything.
  outputContainer.ownerDocument.activeElement.blur();

  const selection = outputContainer.ownerDocument.getSelection();
  const range = document.createRange();
  range.selectNodeContents(node);
  selection.removeAllRanges();
  selection.addRange(range);

  return selection;
}

async function waitForBrowserConsole() {
  return new Promise(resolve => {
    Services.obs.addObserver(function observer(subject) {
      Services.obs.removeObserver(observer, "web-console-created");
      subject.QueryInterface(Ci.nsISupportsString);

      const hud = HUDService.getBrowserConsole();
      ok(hud, "browser console is open");
      is(subject.data, hud.hudId, "notification hudId is correct");

      executeSoon(() => resolve(hud));
    }, "web-console-created");
  });
}

/**
 * Get the state of a console filter.
 *
 * @param {Object} hud
 */
async function getFilterState(hud) {
  const {outputNode} = hud.ui;
  const filterBar = outputNode.querySelector(".webconsole-filterbar-secondary");
  const buttons = filterBar.querySelectorAll("button");
  const result = { };

  for (const button of buttons) {
    const classes = new Set(button.classList.values());
    const checked = classes.has("checked");

    classes.delete("devtools-button");
    classes.delete("checked");

    const category = classes.values().next().value;

    result[category] = checked;
  }

  return result;
}

/**
 * Set the state of a console filter.
 *
 * @param {Object} hud
 * @param {Object} settings
 *        Category settings in the following format:
 *          {
 *            error: true,
 *            warn: true,
 *            log: true,
 *            info: true,
 *            debug: true,
 *            css: false,
 *            netxhr: false,
 *            net: false
 *          }
 */
async function setFilterState(hud, settings) {
  const {outputNode} = hud.ui;
  const filterBar = outputNode.querySelector(".webconsole-filterbar-secondary");

  for (const category in settings) {
    const setActive = settings[category];
    const button = filterBar.querySelector(`.${category}`);

    if (!button) {
      ok(false, `setFilterState() called with a category of ${category}, ` +
                `which doesn't exist.`);
    }

    info(`Setting the ${category} category to ${setActive ? "checked" : "disabled"}`);

    const isChecked = button.classList.contains("checked");

    if (setActive !== isChecked) {
      button.click();

      await waitFor(() => {
        return button.classList.contains("checked") === setActive;
      });
    }
  }
}

/**
 * Reset the filters at the end of a test that has changed them. This is
 * important when using the `--verify` test option as when it is used you need
 * to manually reset the filters.
 *
 * The css, netxhr and net filters are disabled by default.
 *
 * @param {Object} hud
 */
async function resetFilters(hud) {
  info("Resetting filters to their default state");

  const store = hud.ui.wrapper.getStore();
  store.dispatch(wcActions.filtersClear());
}

/**
 * Open the reverse search input by simulating the appropriate keyboard shortcut.
 *
 * @param {Object} hud
 * @returns {DOMNode} The reverse search dom node.
 */
async function openReverseSearch(hud) {
  info("Open the reverse search UI with a keyboard shortcut");
  const onReverseSearchUiOpen = waitFor(() => getReverseSearchElement(hud));
  const isMacOS = AppConstants.platform === "macosx";
  if (isMacOS) {
    EventUtils.synthesizeKey("r", {ctrlKey: true});
  } else {
    EventUtils.synthesizeKey("VK_F9");
  }

  const element = await onReverseSearchUiOpen;
  return element;
}

function getReverseSearchElement(hud) {
  const {outputNode} = hud.ui;
  return outputNode.querySelector(".reverse-search");
}

function getReverseSearchInfoElement(hud) {
  const reverseSearchElement = getReverseSearchElement(hud);
  if (!reverseSearchElement) {
    return null;
  }

  return reverseSearchElement.querySelector(".reverse-search-info");
}

/**
 * Returns a boolean indicating if the reverse search input is focused.
 *
 * @param {WebConsole} hud
 * @returns {Boolean}
 */
function isReverseSearchInputFocused(hud) {
  const {outputNode} = hud.ui;
  const document = outputNode.ownerDocument;
  const documentIsFocused = document.hasFocus();
  const reverseSearchInput = outputNode.querySelector(".reverse-search-input");

  return document.activeElement == reverseSearchInput && documentIsFocused;
}

/**
 * Selects a node in the inspector.
 *
 * @param {Object} toolbox
 * @param {Object} testActor: A test actor registered on the target. Needed to click on
 *                            the content element.
 * @param {String} selector: The selector for the node we want to select.
 */
async function selectNodeWithPicker(toolbox, testActor, selector) {
  const inspector = toolbox.getPanel("inspector");
  const inspectorFront = inspector.inspector;

  const onPickerStarted = inspectorFront.nodePicker.once("picker-started");
  inspectorFront.nodePicker.start();
  await onPickerStarted;

  info(`Picker mode started, now clicking on "${selector}" to select that node`);
  const onPickerStopped = inspectorFront.nodePicker.once("picker-stopped");
  const onInspectorUpdated = inspector.once("inspector-updated");

  testActor.synthesizeMouse({
    selector,
    center: true,
    options: {},
  });

  await onPickerStopped;
  await onInspectorUpdated;
}

/**
 * Clicks on the arrow of a single object inspector node if it exists.
 *
 * @param {HTMLElement} node: Object inspector node (.tree-node)
 */
function expandObjectInspectorNode(node) {
  const arrow = getObjectInspectorNodeArrow(node);
  if (!arrow) {
    ok(false, "Node can't be expanded");
    return;
  }
  arrow.click();
}

/**
 * Retrieve the arrow of a single object inspector node.
 *
 * @param {HTMLElement} node: Object inspector node (.tree-node)
 * @return {HTMLElement|null} the arrow element
 */
function getObjectInspectorNodeArrow(node) {
  return node.querySelector(".arrow");
}

/**
 * Check if a single object inspector node is expandable.
 *
 * @param {HTMLElement} node: Object inspector node (.tree-node)
 * @return {Boolean} true if the node can be expanded
 */
function isObjectInspectorNodeExpandable(node) {
  return !!getObjectInspectorNodeArrow(node);
}

/**
 * Retrieve the nodes for a given object inspector element.
 *
 * @param {HTMLElement} oi: Object inspector element
 * @return {NodeList} the object inspector nodes
 */
function getObjectInspectorNodes(oi) {
  return oi.querySelectorAll(".tree-node");
}

/**
 * Retrieve the "children" nodes for a given object inspector node.
 *
 * @param {HTMLElement} node: Object inspector node (.tree-node)
 * @return {Array<HTMLElement>} the direct children (i.e. the ones that are one level
 *                              deeper than the passed node)
 */
function getObjectInspectorChildrenNodes(node) {
  const getLevel = n => parseInt(n.getAttribute("aria-level"), 10);
  const level = getLevel(node);
  const childLevel = level + 1;
  const children = [];

  let currentNode = node;
  while (currentNode.nextSibling && getLevel(currentNode.nextSibling) === childLevel) {
    currentNode = currentNode.nextSibling;
    children.push(currentNode);
  }

  return children;
}

/**
 * Retrieve the invoke getter button for a given object inspector node.
 *
 * @param {HTMLElement} node: Object inspector node (.tree-node)
 * @return {HTMLElement|null} the invoke button element
 */
function getObjectInspectorInvokeGetterButton(node) {
  return node.querySelector(".invoke-getter");
}

/**
 * Retrieve the first node that match the passed node label, for a given object inspector
 * element.
 *
 * @param {HTMLElement} oi: Object inspector element
 * @param {String} nodeLabel: label of the searched node
 * @return {HTMLElement|null} the Object inspector node with the matching label
 */
function findObjectInspectorNode(oi, nodeLabel) {
  return [...oi.querySelectorAll(".tree-node")].find(node => {
    const label = node.querySelector(".object-label");
    if (!label) {
      return false;
    }
    return label.textContent === nodeLabel;
  });
}

/**
 * Return an array of the label of the autocomplete popup items.
 *
 * @param {AutocompletPopup} popup
 * @returns {Array<String>}
 */
function getAutocompletePopupLabels(popup) {
  return popup.getItems().map(item => item.label);
}

/**
 * Return the "Confirm Dialog" element.
 *
 * @param toolbox
 * @returns {HTMLElement|null}
 */
function getConfirmDialog(toolbox) {
  const {doc} = toolbox;
  return doc.querySelector(".invoke-confirm");
}

/**
 * Returns true if the Confirm Dialog is opened.
 * @param toolbox
 * @returns {Boolean}
 */
function isConfirmDialogOpened(toolbox) {
  const tooltip = getConfirmDialog(toolbox);
  if (!tooltip) {
    return false;
  }

  return tooltip.classList.contains("tooltip-visible");
}

async function selectFrame(dbg, frame) {
  const onScopes = waitForDispatch(dbg, "ADD_SCOPES");
  await dbg.actions.selectFrame(dbg.selectors.getThreadContext(), frame);
  await onScopes;
}

async function pauseDebugger(dbg) {
  info("Waiting for debugger to pause");
  const onPaused = waitForPaused(dbg);
  ContentTask.spawn(gBrowser.selectedBrowser, {}, async function() {
    content.wrappedJSObject.firstCall();
  });
  await onPaused;
}

/**
 * Check that the passed HTMLElement vertically overflows.
 * @param {HTMLElement} container
 * @returns {Boolean}
 */
function hasVerticalOverflow(container) {
  return container.scrollHeight > container.clientHeight;
}

/**
 * Check that the passed HTMLElement is scrolled to the bottom.
 * @param {HTMLElement} container
 * @returns {Boolean}
 */
function isScrolledToBottom(container) {
  if (!container.lastChild) {
    return true;
  }
  const lastNodeHeight = container.lastChild.clientHeight;
  return container.scrollTop + container.clientHeight >=
         container.scrollHeight - lastNodeHeight / 2;
}

/**
 *
 * @param {WebConsole} hud
 * @param {Array<String>} expectedMessages: An array of string representing the messages
 *                        from the output. This can only be a part of the string of the
 *                        message.
 *                        Start the string with "▶︎⚠ " or "▼⚠ " to indicate that the
 *                        message is a warningGroup (with respectively an open or
 *                        collapsed arrow).
 *                        Start the string with "|︎ " to indicate that the message is
 *                        inside a group and should be indented.
 */
function checkConsoleOutputForWarningGroup(hud, expectedMessages) {
  const messages = findMessages(hud, "");
  is(messages.length, expectedMessages.length, "Got the expected number of messages");

  const isInWarningGroup = index => {
    const message = expectedMessages[index];
    if (!message.startsWith("|")) {
      return false;
    }
    const groups = expectedMessages.slice(0, index)
      .reverse()
      .filter(m => !m.startsWith("|"));
    if (groups.length === 0) {
      ok(false, "Unexpected structure: an indented message isn't in a group");
    }

    return groups[0].startsWith("▶︎⚠") || groups[0].startsWith("▼⚠");
  };

  expectedMessages.forEach((expectedMessage, i) => {
    const message = messages[i];
    info(`Checking "${expectedMessage}"`);

    // Collapsed Warning group
    if (expectedMessage.startsWith("▶︎⚠")) {
      is(message.querySelector(".arrow").getAttribute("aria-expanded"), "false",
        "There's a collapsed arrow");
      is(message.querySelector(".indent").getAttribute("data-indent"), "0",
        "The warningGroup has the expected indent");
      expectedMessage = expectedMessage.replace("▶︎⚠ ", "");
    }

    // Expanded Warning group
    if (expectedMessage.startsWith("▼︎⚠")) {
      is(message.querySelector(".arrow").getAttribute("aria-expanded"), "true",
        "There's an expanded arrow");
      is(message.querySelector(".indent").getAttribute("data-indent"), "0",
        "The warningGroup has the expected indent");
      expectedMessage = expectedMessage.replace("▼︎⚠ ", "");
    }

    // Collapsed console.group
    if (expectedMessage.startsWith("▶︎")) {
      is(message.querySelector(".arrow").getAttribute("aria-expanded"), "false",
        "There's a collapsed arrow");
      expectedMessage = expectedMessage.replace("▶︎ ", "");
    }

    // Expanded console.group
    if (expectedMessage.startsWith("▼")) {
      is(message.querySelector(".arrow").getAttribute("aria-expanded"), "true",
        "There's an expanded arrow");
      expectedMessage = expectedMessage.replace("▼ ", "");
    }

    // In-group message
    if (expectedMessage.startsWith("|")) {
      if (isInWarningGroup(i)) {
        is(message.querySelector(".indent.warning-indent").getAttribute("data-indent"),
          "1", "The message has the expected indent");
      }

      expectedMessage = expectedMessage.replace("| ", "");
    }

    ok(message.textContent.trim().includes(expectedMessage.trim()), `Message includes ` +
      `the expected "${expectedMessage}" content - "${message.textContent.trim()}"`);
  });
}

/**
 * Check that there is a message with the specified text that has the specified
 * stack information.
 * @param {WebConsole} hud
 * @param {string} text
 *        message substring to look for
 * @param {Array<number>} frameLines
 *        line numbers of the frames expected in the stack
 */
async function checkMessageStack(hud, text, frameLines) {
  const msgNode = await waitFor(() => findMessage(hud, text));
  ok(!msgNode.classList.contains("open"), `Error logged not expanded`);

  const button = await waitFor(() => msgNode.querySelector(".collapse-button"));
  button.click();

  const framesNode = await waitFor(() => msgNode.querySelector(".frames"));
  const frameNodes = framesNode.querySelectorAll(".frame");
  ok(frameNodes.length == frameLines.length, `Found ${frameLines.length} frames`);
  for (let i = 0; i < frameLines.length; i++) {
    ok(frameNodes[i].querySelector(".line").textContent == "" + frameLines[i],
       `Found line ${frameLines[i]} for frame #${i}`);
  }
}
