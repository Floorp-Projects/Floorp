/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global STATE_CHECKED, EXT_STATE_EDITABLE, nsIAccessibleStateChangeEvent,
          EVENT_STATE_CHANGE */

'use strict';

loadScripts({ name: 'role.js', dir: MOCHITESTS_DIR },
            { name: 'states.js', dir: MOCHITESTS_DIR });

function checkStateChangeEvent(event, state, isExtraState, isEnabled) {
  let scEvent = event.QueryInterface(nsIAccessibleStateChangeEvent);
  is(scEvent.state, state, 'Correct state of the statechange event.');
  is(scEvent.isExtraState, isExtraState,
    'Correct extra state bit of the statechange event.');
  is(scEvent.isEnabled, isEnabled, 'Correct state of statechange event state');
}

// Insert mock source into the iframe to be able to verify the right document
// body id.
let iframeSrc = `data:text/html,
  <html>
    <head>
      <meta charset='utf-8'/>
      <title>Inner Iframe</title>
    </head>
    <body id='iframe'></body>
  </html>`;

/**
 * Test state change event and its interface:
 *   - state
 *   - isExtraState
 *   - isEnabled
 */
addAccessibleTask(`
  <iframe id="iframe" src="${iframeSrc}"></iframe>
  <input id="checkbox" type="checkbox" />`, function*(browser) {
  // Test state change
  let onStateChange = waitForEvent(EVENT_STATE_CHANGE, 'checkbox');
  // Set checked for a checkbox.
  yield ContentTask.spawn(browser, {}, () =>
    content.document.getElementById('checkbox').checked = true);
  let event = yield onStateChange;

  checkStateChangeEvent(event, STATE_CHECKED, false, true);
  testStates(event.accessible, STATE_CHECKED, 0);

  // Test extra state
  onStateChange = waitForEvent(EVENT_STATE_CHANGE, 'iframe');
  // Set design mode on.
  yield ContentTask.spawn(browser, {}, () =>
    content.document.getElementById('iframe').contentDocument.designMode = 'on');
  event = yield onStateChange;

  checkStateChangeEvent(event, EXT_STATE_EDITABLE, true, true);
  testStates(event.accessible, 0, EXT_STATE_EDITABLE);
});
