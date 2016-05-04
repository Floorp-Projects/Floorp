/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

/* global EVENT_SHOW */

loadScripts({ name: 'role.js', dir: MOCHITESTS_DIR });

addAccessibleTask(`
  <canvas id="canvas">
    <div id="dialog" role="dialog" style="display: none;"></div>
  </canvas>`, function*(browser, accDoc) {
  let canvas = findAccessibleChildByID(accDoc, 'canvas');
  let dialog = findAccessibleChildByID(accDoc, 'dialog');

  testAccessibleTree(canvas, { CANVAS: [] });

  let onShow = waitForEvent(EVENT_SHOW, 'dialog');
  yield invokeSetStyle(browser, 'dialog', 'display', 'block');
  yield onShow;

  testAccessibleTree(dialog, { DIALOG: [] });
});
