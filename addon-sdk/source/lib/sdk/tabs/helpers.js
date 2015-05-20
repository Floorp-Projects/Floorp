/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

module.metadata = {
  'stability': 'unstable'
};


// NOTE: This file should only export Tab instances


const { getTabForContentWindow, getTabForBrowser: getRawTabForBrowser } = require('./utils');
const { modelFor } = require('../model/core');

function getTabForWindow(win) {
  let tab = getTabForContentWindow(win);
  // We were unable to find the related tab!
  if (!tab)
    return null;

  return modelFor(tab);
}
exports.getTabForWindow = getTabForWindow;

exports.getTabForRawTab = modelFor;

function getTabForBrowser(browser) {
  return modelFor(getRawTabForBrowser(browser)) || null;
}
exports.getTabForBrowser = getTabForBrowser;
