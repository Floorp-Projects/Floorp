/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global EVENT_SHOW */

'use strict';

/**
 * Test show event
 */
addAccessibleTask('<div id="div" style="visibility: hidden;"></div>',
  function*(browser) {
    let onShow = waitForEvent(EVENT_SHOW, 'div');
    yield invokeSetStyle(browser, 'div', 'visibility', 'visible');
    yield onShow;
  });
