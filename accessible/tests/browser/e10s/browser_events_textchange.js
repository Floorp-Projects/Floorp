/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function checkTextChangeEvent(
  event,
  id,
  text,
  start,
  end,
  isInserted,
  isFromUserInput
) {
  let tcEvent = event.QueryInterface(nsIAccessibleTextChangeEvent);
  is(tcEvent.start, start, `Correct start offset for ${prettyName(id)}`);
  is(tcEvent.length, end - start, `Correct length for ${prettyName(id)}`);
  is(
    tcEvent.isInserted,
    isInserted,
    `Correct isInserted flag for ${prettyName(id)}`
  );
  is(tcEvent.modifiedText, text, `Correct text for ${prettyName(id)}`);
  is(
    tcEvent.isFromUserInput,
    isFromUserInput,
    `Correct value of isFromUserInput for ${prettyName(id)}`
  );
  ok(
    tcEvent.accessibleDocument instanceof nsIAccessibleDocument,
    "Accessible document not present."
  );
}

async function changeText(browser, id, value, events) {
  let onEvents = waitForOrderedEvents(
    events.map(({ isInserted }) => {
      let eventType = isInserted ? EVENT_TEXT_INSERTED : EVENT_TEXT_REMOVED;
      return [eventType, id];
    })
  );
  // Change text in the subtree.
  await invokeContentTask(browser, [id, value], (contentId, contentValue) => {
    content.document.getElementById(
      contentId
    ).firstChild.textContent = contentValue;
  });
  let resolvedEvents = await onEvents;

  events.forEach(({ isInserted, str, offset }, idx) =>
    checkTextChangeEvent(
      resolvedEvents[idx],
      id,
      str,
      offset,
      offset + str.length,
      isInserted,
      false
    )
  );
}

async function removeTextFromInput(browser, id, value, start, end) {
  let onTextRemoved = waitForEvent(EVENT_TEXT_REMOVED, id);
  // Select text and delete it.
  await invokeContentTask(
    browser,
    [id, start, end],
    (contentId, contentStart, contentEnd) => {
      let el = content.document.getElementById(contentId);
      el.focus();
      el.setSelectionRange(contentStart, contentEnd);
    }
  );
  await invokeContentTask(browser, [], () => {
    const { ContentTaskUtils } = ChromeUtils.import(
      "resource://testing-common/ContentTaskUtils.jsm"
    );
    const EventUtils = ContentTaskUtils.getEventUtils(content);
    EventUtils.sendChar("VK_DELETE", content);
  });

  let event = await onTextRemoved;
  checkTextChangeEvent(event, id, value, start, end, false, true);
}

/**
 * Test text change event and its interface:
 *   - start
 *   - length
 *   - isInserted
 *   - modifiedText
 *   - isFromUserInput
 */
addAccessibleTask(
  `
  <p id="p">abc</p>
  <input id="input" value="input" />`,
  async function(browser) {
    let events = [
      { isInserted: false, str: "abc", offset: 0 },
      { isInserted: true, str: "def", offset: 0 },
    ];
    await changeText(browser, "p", "def", events);

    events = [{ isInserted: true, str: "DEF", offset: 2 }];
    await changeText(browser, "p", "deDEFf", events);

    // Test isFromUserInput property.
    await removeTextFromInput(browser, "input", "n", 1, 2);
  },
  { iframe: true, remoteIframe: true }
);
