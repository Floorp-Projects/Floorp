/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

/**
 * Test caret move event and its interface:
 *   - caretOffset
 */
addAccessibleTask('<input id="textbox" value="hello"/>', async function(browser) {
  let onCaretMoved = waitForEvent(EVENT_TEXT_CARET_MOVED, 'textbox');
  await invokeFocus(browser, 'textbox');
  let event = await onCaretMoved;

  let caretMovedEvent = event.QueryInterface(nsIAccessibleCaretMoveEvent);
  is(caretMovedEvent.caretOffset, 5,
    'Correct caret offset.');
});
