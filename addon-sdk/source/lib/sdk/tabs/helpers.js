/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const { getTabForContentWindow } = require('./utils');
const { Tab } = require('./tab');

function getTabForWindow(win) {
  let tab = getTabForContentWindow(win);
  // We were unable to find the related tab!
  if (!tab)
    return null;

  return Tab({ tab: tab });
}
exports.getTabForWindow = getTabForWindow;
