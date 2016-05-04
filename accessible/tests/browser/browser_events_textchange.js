/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global EVENT_TEXT_INSERTED, EVENT_TEXT_REMOVED,
          nsIAccessibleTextChangeEvent */

'use strict';

function checkTextChangeEvent(event, id, text, start, end, isInserted, isFromUserInput) {
  let tcEvent = event.QueryInterface(nsIAccessibleTextChangeEvent);
  is(tcEvent.start, start, `Correct start offset for ${prettyName(id)}`);
  is(tcEvent.length, end - start, `Correct length for ${prettyName(id)}`);
  is(tcEvent.isInserted, isInserted,
    `Correct isInserted flag for ${prettyName(id)}`);
  is(tcEvent.modifiedText, text, `Correct text for ${prettyName(id)}`);
  is(tcEvent.isFromUserInput, isFromUserInput,
    `Correct value of isFromUserInput for ${prettyName(id)}`);
}

function* changeText(browser, id, value, events) {
  let onEvents = waitForMultipleEvents(events.map(({ isInserted }) => {
    let eventType = isInserted ? EVENT_TEXT_INSERTED : EVENT_TEXT_REMOVED;
    return { id, eventType };
  }));
  // Change text in the subtree.
  yield ContentTask.spawn(browser, { id, value }, ({ id, value }) =>
    content.document.getElementById(id).firstChild.textContent = value);
  let resolvedEvents = yield onEvents;

  events.forEach(({ isInserted, str, offset }, idx) =>
    checkTextChangeEvent(resolvedEvents[idx],
      id, str, offset, offset + str.length, isInserted, false));
}

function* removeTextFromInput(browser, id, value, start, end) {
  let onTextRemoved = waitForEvent(EVENT_TEXT_REMOVED, id);
  // Select text and delete it.
  yield ContentTask.spawn(browser, { id, start, end }, ({ id, start, end }) => {
    let el = content.document.getElementById(id);
    el.focus();
    el.setSelectionRange(start, end);
  });
  yield BrowserTestUtils.sendChar('VK_DELETE', browser);

  let event = yield onTextRemoved;
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
addAccessibleTask(`
  <p id="p">abc</p>
  <input id="input" value="input" />`, function*(browser) {
  let events = [
    { isInserted: false, str: 'abc', offset: 0 },
    { isInserted: true, str: 'def', offset: 0 }
  ];
  yield changeText(browser, 'p', 'def', events);

  events = [{ isInserted: true, str: 'DEF', offset: 2 }];
  yield changeText(browser, 'p', 'deDEFf', events);

  // Test isFromUserInput property.
  yield removeTextFromInput(browser, 'input', 'n', 1, 2);
});
