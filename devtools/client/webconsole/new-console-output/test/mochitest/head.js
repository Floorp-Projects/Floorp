/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from ../../../../framework/test/shared-head.js */
/* exported WCUL10n, openNewTabAndConsole, waitForMessages, waitFor, findMessage,
   openContextMenu, hideContextMenu, loadDocument, hasFocus,
   waitForNodeMutation, testOpenInDebugger, checkClickOnNode */

"use strict";

// shared-head.js handles imports, constants, and utility functions
// Load the shared-head file first.
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/framework/test/shared-head.js",
  this);

var {HUDService} = require("devtools/client/webconsole/hudservice");
var WCUL10n = require("devtools/client/webconsole/webconsole-l10n");

Services.prefs.setBoolPref("devtools.webconsole.new-frontend-enabled", true);
registerCleanupFunction(function* () {
  Services.prefs.clearUserPref("devtools.webconsole.new-frontend-enabled");

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
 * @return Promise
 *         Resolves when the tab has been added, loaded and the toolbox has been opened.
 *         Resolves to the toolbox.
 */
var openNewTabAndConsole = Task.async(function* (url) {
  let toolbox = yield openNewTabAndToolbox(url, "webconsole");
  let hud = toolbox.getCurrentPanel().hud;
  hud.jsterm._lazyVariablesView = false;
  return hud;
});

/**
 * Wait for messages in the web console output, resolving once they are received.
 *
 * @param object options
 *        - hud: the webconsole
 *        - messages: Array[Object]. An array of messages to match.
            Current supported options:
 *            - text: Exact text match in .message-body
 */
function waitForMessages({ hud, messages }) {
  return new Promise(resolve => {
    let numMatched = 0;
    let receivedLog = hud.ui.on("new-messages",
      function messagesReceived(e, newMessages) {
        for (let message of messages) {
          if (message.matched) {
            continue;
          }

          for (let newMessage of newMessages) {
            let messageBody = newMessage.node.querySelector(".message-body");
            if (messageBody.textContent.includes(message.text)) {
              numMatched++;
              message.matched = true;
              info("Matched a message with text: " + message.text +
                ", still waiting for " + (messages.length - numMatched) + " messages");
              break;
            }
          }

          if (numMatched === messages.length) {
            hud.ui.off("new-messages", messagesReceived);
            resolve(receivedLog);
            return;
          }
        }
      });
  });
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
  return new Promise(resolve => {
    browser.addEventListener("load", resolve, {capture: true, once: true});
    BrowserTestUtils.loadURI(gBrowser.selectedBrowser, url);
  });
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
function* testOpenInDebugger(hud, toolbox, text) {
  info(`Finding message for open-in-debugger test; text is "${text}"`);
  let messageNode = yield waitFor(() => findMessage(hud, text));
  let frameLinkNode = messageNode.querySelector(".message-location .frame-link");
  ok(frameLinkNode, "The message does have a location link");
  yield checkClickOnNode(hud, toolbox, frameLinkNode);
}

/**
 * Helper function for testOpenInDebugger.
 */
function* checkClickOnNode(hud, toolbox, frameLinkNode) {
  info("checking click on node location");

  let url = frameLinkNode.getAttribute("data-url");
  ok(url, `source url found ("${url}")`);

  let line = frameLinkNode.getAttribute("data-line");
  ok(line, `source line found ("${line}")`);

  let onSourceInDebuggerOpened = once(hud.ui, "source-in-debugger-opened");

  EventUtils.sendMouseEvent({ type: "click" },
    frameLinkNode.querySelector(".frame-link-filename"));

  yield onSourceInDebuggerOpened;

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
