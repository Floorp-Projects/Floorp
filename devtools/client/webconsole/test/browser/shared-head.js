/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Helper methods for finding messages in the virtualized output of the
 * webconsole. This file can be safely required from other panel test
 * files.
 */

"use strict";

/* eslint-disable no-unused-vars */

// Assume that shared-head is always imported before this file
/* import-globals-from ../../../shared/test/shared-head.js */

/**
 * Find a message with given messageId in the output, scrolling through the
 * output from top to bottom in order to make sure the messages are actually
 * rendered.
 *
 * @param object hud
 *        The web console.
 * @param messageId
 *        A message ID to look for. This could be baked into the selector, but
 *        is provided as a convenience.
 * @return {Node} the node corresponding the found message
 */
async function findMessageVirtualizedById({ hud, messageId }) {
  if (!messageId) {
    throw new Error("messageId parameter is required");
  }

  const elements = await findMessagesVirtualized({
    hud,
    expectedCount: 1,
    messageId,
  });
  return elements.at(-1);
}

/**
 * Find the last message with given message type in the output, scrolling
 * through the output from top to bottom in order to make sure the messages are
 * actually rendered.
 *
 * @param object hud
 *        The web console.
 * @param string text
 *        A substring that can be found in the message.
 * @param string typeSelector
 *        A part of selector for the message, to specify the message type.
 * @return {Node} the node corresponding the found message
 */
async function findMessageVirtualizedByType({ hud, text, typeSelector }) {
  const elements = await findMessagesVirtualizedByType({
    hud,
    text,
    typeSelector,
    expectedCount: 1,
  });
  return elements.at(-1);
}

/**
 * Find all messages in the output, scrolling through the output from top
 * to bottom in order to make sure the messages are actually rendered.
 *
 * @param object hud
 *        The web console.
 * @return {Array} all of the message nodes in the console output. Some of
 *        these may be stale from having been scrolled out of view.
 */
async function findAllMessagesVirtualized(hud) {
  return findMessagesVirtualized({ hud });
}

// This is just a reentrancy guard. Because findMessagesVirtualized mucks
// around with the scroll position, if we do something like
//   let promise1 = findMessagesVirtualized(...);
//   let promise2 = findMessagesVirtualized(...);
//   await promise1;
//   await promise2;
// then the two calls will end up messing up each other's expected scroll
// position, at which point they could get stuck. This lets us throw an
// error when that happens.
let gInFindMessagesVirtualized = false;
// And this lets us get a little more information in the error - it just holds
// the stack of the prior call.
let gFindMessagesVirtualizedStack = null;

/**
 * Find multiple messages in the output, scrolling through the output from top
 * to bottom in order to make sure the messages are actually rendered.
 *
 * @param object options
 * @param object options.hud
 *        The web console.
 * @param options.text [optional]
 *        A substring that can be found in the message.
 * @param options.typeSelector
 *        A part of selector for the message, to specify the message type.
 * @param options.expectedCount [optional]
 *        The number of messages to get. This lets us stop scrolling early if
 *        we find that number of messages.
 * @return {Array} all of the message nodes in the console output matching the
 *        provided filters. If expectedCount is greater than 1, or equal to -1,
 *        some of these may be stale from having been scrolled out of view.
 */
async function findMessagesVirtualizedByType({
  hud,
  text,
  typeSelector,
  expectedCount,
}) {
  if (!typeSelector) {
    throw new Error("typeSelector parameter is required");
  }
  if (!typeSelector.startsWith(".")) {
    throw new Error("typeSelector should start with a dot e.g. `.result`");
  }

  return findMessagesVirtualized({
    hud,
    text,
    selector: ".message" + typeSelector,
    expectedCount,
  });
}

/**
 * Find multiple messages in the output, scrolling through the output from top
 * to bottom in order to make sure the messages are actually rendered.
 *
 * @param object options
 * @param object options.hud
 *        The web console.
 * @param options.text [optional]
 *        A substring that can be found in the message.
 * @param options.selector [optional]
 *        The selector to use in finding the message.
 * @param options.expectedCount [optional]
 *        The number of messages to get. This lets us stop scrolling early if
 *        we find that number of messages.
 * @param options.messageId [optional]
 *        A message ID to look for. This could be baked into the selector, but
 *        is provided as a convenience.
 * @return {Array} all of the message nodes in the console output matching the
 *        provided filters. If expectedCount is greater than 1, or equal to -1,
 *        some of these may be stale from having been scrolled out of view.
 */
async function findMessagesVirtualized({
  hud,
  text,
  selector,
  expectedCount,
  messageId,
}) {
  if (text === undefined) {
    text = "";
  }
  if (selector === undefined) {
    selector = ".message";
  }
  if (expectedCount === undefined) {
    expectedCount = -1;
  }

  const outputNode = hud.ui.outputNode;
  const scrollport = outputNode.querySelector(".webconsole-output");

  function getVisibleMessageIds() {
    return JSON.parse(scrollport.getAttribute("data-visible-messages"));
  }

  function getVisibleMessageMap() {
    return new Map(
      JSON.parse(scrollport.getAttribute("data-visible-messages")).map(
        (id, i) => [id, i]
      )
    );
  }

  function getMessageIndex(message) {
    return getVisibleMessageIds().indexOf(
      message.getAttribute("data-message-id")
    );
  }

  function getNextMessageId(prevMessage) {
    const visible = getVisibleMessageIds();
    let index = 0;
    if (prevMessage) {
      const lastId = prevMessage.getAttribute("data-message-id");
      index = visible.indexOf(lastId);
      if (index === -1) {
        throw new Error(
          `Tried to get next message ID for message that doesn't exist. Last seen ID: ${lastId}, all visible ids: [${visible.join(
            ", "
          )}]`
        );
      }
    }
    if (index + 1 >= visible.length) {
      return null;
    }
    return visible[index + 1];
  }

  if (gInFindMessagesVirtualized) {
    throw new Error(
      `findMessagesVirtualized was re-entered somehow. This is not allowed. Other stack: [${gFindMessagesVirtualizedStack}]`
    );
  }
  try {
    gInFindMessagesVirtualized = true;
    gFindMessagesVirtualizedStack = new Error().stack;
    // The console output will automatically scroll to the bottom of the
    // scrollport in certain circumstances. Because we need to scroll the
    // output to find all messages, we need to disable this. This attribute
    // controls that.
    scrollport.setAttribute("disable-autoscroll", "");

    // This array is here purely for debugging purposes. We collect the indices
    // of every element we see in order to validate that we don't have any gaps
    // in the list.
    const allIndices = [];

    const allElements = [];
    const seenIds = new Set();
    let lastItem = null;
    while (true) {
      if (scrollport.scrollHeight > scrollport.clientHeight) {
        if (!lastItem && scrollport.scrollTop != 0) {
          // For simplicity's sake, we always start from the top of the output.
          scrollport.scrollTop = 0;
        } else if (!lastItem && scrollport.scrollTop == 0) {
          // We want to make sure that we actually change the scroll position
          // here, because we're going to wait for an update below regardless,
          // just to flush out any changes that may have just happened. If we
          // don't do this, and there were no changes before this function was
          // called, then we'll just hang on the promise below.
          scrollport.scrollTop = 1;
        } else {
          // This is the core of the loop. Scroll down to the bottom of the
          // current scrollport, wait until we see the element after the last
          // one we've seen, and then harvest the messages that are displayed.
          scrollport.scrollTop = scrollport.scrollTop + scrollport.clientHeight;
        }

        // Wait for something to happen in the output before checking for our
        // expected next message.
        await new Promise(resolve =>
          hud.ui.once("lazy-message-list-updated-or-noop", resolve)
        );

        try {
          await waitFor(async () => {
            const nextMessageId = getNextMessageId(lastItem);
            if (
              nextMessageId === undefined ||
              scrollport.querySelector(`[data-message-id="${nextMessageId}"]`)
            ) {
              return true;
            }

            // After a scroll, we typically expect to get an updated list of
            // elements. However, we have some slack at the top of the list,
            // because we draw elements above and below the actual scrollport to
            // avoid white flashes when async scrolling.
            const scrollTarget = scrollport.scrollTop + scrollport.clientHeight;
            scrollport.scrollTop = scrollTarget;
            await new Promise(resolve =>
              hud.ui.once("lazy-message-list-updated-or-noop", resolve)
            );
            return false;
          });
        } catch (e) {
          throw new Error(
            `Failed waiting for next message ID (${getNextMessageId(
              lastItem
            )}) Visible messages: [${[
              ...scrollport.querySelectorAll(".message"),
            ].map(el => el.getAttribute("data-message-id"))}]`
          );
        }
      }

      const bottomPlaceholder = scrollport.querySelector(
        ".lazy-message-list-bottom"
      );
      if (!bottomPlaceholder) {
        // When there are no messages in the output, there is also no
        // top/bottom placeholder. There's nothing more to do at this point,
        // so break and return.
        break;
      }

      lastItem = bottomPlaceholder.previousSibling;

      // This chunk is just validating that we have no gaps in our output so
      // far.
      const indices = [...scrollport.querySelectorAll("[data-message-id]")]
        .filter(
          el => el !== scrollport.firstChild && el !== scrollport.lastChild
        )
        .map(el => getMessageIndex(el));
      allIndices.push(...indices);
      allIndices.sort((lhs, rhs) => lhs - rhs);
      for (let i = 1; i < allIndices.length; i++) {
        if (
          allIndices[i] != allIndices[i - 1] &&
          allIndices[i] != allIndices[i - 1] + 1
        ) {
          throw new Error(
            `Gap detected in virtualized webconsole output between ${
              allIndices[i - 1]
            } and ${allIndices[i]}. Indices: ${allIndices.join(",")}`
          );
        }
      }

      const messages = scrollport.querySelectorAll(selector);
      const filtered = [...messages].filter(
        el =>
          // Core user filters:
          el.textContent.includes(text) &&
          (!messageId || el.getAttribute("data-message-id") === messageId) &&
          // Make sure we don't collect duplicate messages:
          !seenIds.has(el.getAttribute("data-message-id"))
      );
      allElements.push(...filtered);
      for (const message of filtered) {
        seenIds.add(message.getAttribute("data-message-id"));
      }

      if (expectedCount >= 0 && allElements.length >= expectedCount) {
        break;
      }

      // If the bottom placeholder has 0 height, it means we've scrolled to the
      // bottom and output all the items.
      if (bottomPlaceholder.getBoundingClientRect().height == 0) {
        break;
      }

      await waitForTime(0);
    }

    // Finally, we get the map of message IDs to indices within the output, and
    // sort the message nodes according to that index. They can come in out of
    // order for a number of reasons (we continue rendering any messages that
    // have been expanded, and we always render the topmost and bottommost
    // messages for a11y reasons.)
    const idsToIndices = getVisibleMessageMap();
    allElements.sort(
      (lhs, rhs) =>
        idsToIndices.get(lhs.getAttribute("data-message-id")) -
        idsToIndices.get(rhs.getAttribute("data-message-id"))
    );
    return allElements;
  } finally {
    scrollport.removeAttribute("disable-autoscroll");
    gInFindMessagesVirtualized = false;
    gFindMessagesVirtualizedStack = null;
  }
}

/**
 * Find the last message with given message type in the output.
 *
 * @param object hud
 *        The web console.
 * @param string text
 *        A substring that can be found in the message.
 * @param string typeSelector
 *        A part of selector for the message, to specify the message type.
 * @return {Node} the node corresponding the found message, otherwise undefined
 */
function findMessageByType(hud, text, typeSelector) {
  const elements = findMessagesByType(hud, text, typeSelector);
  return elements.at(-1);
}

/**
 * Find multiple messages with given message type in the output.
 *
 * @param object hud
 *        The web console.
 * @param string text
 *        A substring that can be found in the message.
 * @param string typeSelector
 *        A part of selector for the message, to specify the message type.
 * @return {Array} The nodes corresponding the found messages
 */
function findMessagesByType(hud, text, typeSelector) {
  if (!typeSelector) {
    throw new Error("typeSelector parameter is required");
  }
  if (!typeSelector.startsWith(".")) {
    throw new Error("typeSelector should start with a dot e.g. `.result`");
  }

  const selector = ".message" + typeSelector;
  const messages = hud.ui.outputNode.querySelectorAll(selector);
  const elements = Array.from(messages).filter(el =>
    el.textContent.includes(text)
  );
  return elements;
}

/**
 * Find all messages in the output.
 *
 * @param object hud
 *        The web console.
 * @return {Array} The nodes corresponding the found messages
 */
function findAllMessages(hud) {
  const messages = hud.ui.outputNode.querySelectorAll(".message");
  return Array.from(messages);
}

/**
 * Find a part of the last message with given message type in the output.
 *
 * @param object hud
 *        The web console.
 * @param object options
 *        - text : {String} A substring that can be found in the message.
 *        - typeSelector: {String} A part of selector for the message,
 *                                 to specify the message type.
 *        - partSelector: {String} A selector for the part of the message.
 * @return {Node} the node corresponding the found part, otherwise undefined
 */
function findMessagePartByType(hud, options) {
  const elements = findMessagePartsByType(hud, options);
  return elements.at(-1);
}

/**
 * Find parts of multiple messages with given message type in the output.
 *
 * @param object hud
 *        The web console.
 * @param object options
 *        - text : {String} A substring that can be found in the message.
 *        - typeSelector: {String} A part of selector for the message,
 *                                 to specify the message type.
 *        - partSelector: {String} A selector for the part of the message.
 * @return {Array} The nodes corresponding the found parts
 */
function findMessagePartsByType(hud, { text, typeSelector, partSelector }) {
  if (!typeSelector) {
    throw new Error("typeSelector parameter is required");
  }
  if (!typeSelector.startsWith(".")) {
    throw new Error("typeSelector should start with a dot e.g. `.result`");
  }
  if (!partSelector) {
    throw new Error("partSelector parameter is required");
  }

  const selector = ".message" + typeSelector + " " + partSelector;
  const parts = hud.ui.outputNode.querySelectorAll(selector);
  const elements = Array.from(parts).filter(el =>
    el.textContent.includes(text)
  );
  return elements;
}

/**
 * Type-specific wrappers for findMessageByType and findMessagesByType.
 *
 * @param object hud
 *        The web console.
 * @param string text
 *        A substring that can be found in the message.
 * @param string extraSelector [optional]
 *        An extra part of selector for the message, in addition to
 *        type-specific selector.
 * @return {Node|Array} See findMessageByType or findMessagesByType.
 */
function findEvaluationResultMessage(hud, text, extraSelector = "") {
  return findMessageByType(hud, text, ".result" + extraSelector);
}
function findEvaluationResultMessages(hud, text, extraSelector = "") {
  return findMessagesByType(hud, text, ".result" + extraSelector);
}
function findErrorMessage(hud, text, extraSelector = "") {
  return findMessageByType(hud, text, ".error" + extraSelector);
}
function findErrorMessages(hud, text, extraSelector = "") {
  return findMessagesByType(hud, text, ".error" + extraSelector);
}
function findWarningMessage(hud, text, extraSelector = "") {
  return findMessageByType(hud, text, ".warn" + extraSelector);
}
function findWarningMessages(hud, text, extraSelector = "") {
  return findMessagesByType(hud, text, ".warn" + extraSelector);
}
function findConsoleAPIMessage(hud, text, extraSelector = "") {
  return findMessageByType(hud, text, ".console-api" + extraSelector);
}
function findConsoleAPIMessages(hud, text, extraSelector = "") {
  return findMessagesByType(hud, text, ".console-api" + extraSelector);
}
function findNetworkMessage(hud, text, extraSelector = "") {
  return findMessageByType(hud, text, ".network" + extraSelector);
}
function findNetworkMessages(hud, text, extraSelector = "") {
  return findMessagesByType(hud, text, ".network" + extraSelector);
}
function findTracerMessages(hud, text, extraSelector = "") {
  return findMessagesByType(hud, text, ".jstracer" + extraSelector);
}
